// gpu_tile_download.h — Safe GPU tile-row download and write-back utility.
// Shared across Waifu2x / SRMD / RealCUGAN / RealSR backends.
//
// Problem (before this fix)
// -------------------------
// Each backend's GPU path built an ncnn::Mat that *wrapped an external
// pointer* straight into the output image buffer:
//
//     out = ncnn::Mat(out_gpu.w, out_gpu.h,
//                     (unsigned char*)outimage.data
//                         + yi * scale * TILE_SIZE_Y * w * scale * channels,
//                     (size_t)channels, 1);
//     cmd.record_clone(out_gpu, out, opt);
//
// When the last tile row is shorter than a full TILE_SIZE_Y, or when the
// model output size / border padding diverges from the hard-coded formula,
// the external-pointer Mat's declared dimensions can disagree with the
// available space in outimage.  On Android (and any platform where ncnn's
// VkCompute honours the destination's external data pointer as-is) this
// leads to:
//
//   • silent pixel corruption (write into the wrong row / channel), or
//   • out-of-bounds writes / use-after-free when the VkAllocator recycles
//     the staging buffer.
//
// Fix
// ---
// Always download into a *temporary* CPU Mat that ncnn owns, then copy the
// result into outimage at the correct offset with an explicit, bounds-safe
// row-by-row memcpy (int8/fp16 path) or to_pixels() with stride (float
// path).  The offset is computed from the *actual* tile boundary
// (out_tile_y0) that the caller already computed, not from a recomputed
// fixed formula.

#ifndef GPU_TILE_DOWNLOAD_H
#define GPU_TILE_DOWNLOAD_H

#include <cstring>
#include <cstddef>
#include <algorithm>
#include <vector>

#include "tile_row_copy.h"

// The caller is expected to have already included the ncnn headers
// (mat.h, gpu.h / vulkan headers) so that ncnn::Mat, ncnn::VkMat,
// ncnn::VkCompute and ncnn::Option are visible.

// ---------------------------------------------------------------------------
// Safely download one full-width tile row from the GPU and write it back
// into the output image at the correct vertical offset.
//
// Parameters
// ----------
//   out_gpu       VkMat that the postproc shader filled (full output width).
//   outimage      Destination CPU image (w*scale x h*scale x channels,
//                 interleaved uint8).
//   out_tile_y0   Y-start of this tile row in *input* image coordinates.
//                 Must be the same value used when creating out_gpu.
//   scale         Upscale factor (1/2/3/4).
//   w             Input image width.
//   channels      Number of interleaved channels (3 = RGB, 4 = RGBA).
//   fp16_int8     True when out_gpu was created with int8/fp16 storage
//                 (elemsize == channels, data already in uint8 pixel format).
//   cmd           The VkCompute command buffer used for the download.
//   opt           ncnn::Option carrying the staging allocator.
// ---------------------------------------------------------------------------
static inline void download_gpu_tile_to_output(
    const ncnn::VkMat& out_gpu,
    ncnn::Mat& outimage,
    int out_tile_y0,
    int scale,
    int w,
    int channels,
    bool fp16_int8,
    ncnn::VkCompute& cmd,
    const ncnn::Option& opt)
{
    // ---- 1. Download to a *temporary* CPU Mat owned by ncnn. ----
    // This avoids constructing an external-pointer ncnn::Mat whose declared
    // size might disagree with the available space in outimage for edge
    // tiles.
    ncnn::Mat out;
    cmd.record_clone(out_gpu, out, opt);
    cmd.submit_and_wait();

    if (out.empty())
        return;

    // ---- 2. Compute the write-back destination inside outimage. ----
    // Use the *actual* tile boundary (out_tile_y0), not a recomputed formula.
    const size_t dst_stride  = (size_t)w * scale * channels;          // bytes per output row
    const size_t dst_y_start = (size_t)out_tile_y0 * scale;           // first output row of this tile

    // Clamp against the real output image height so a mismatched
    // out_tile_y0 can never produce an out-of-bounds base pointer.
    if (dst_y_start >= (size_t)outimage.h)
        return;

    const size_t dst_capacity = ((size_t)outimage.h - dst_y_start) * dst_stride;
    unsigned char* dst       = (unsigned char*)outimage.data
                             + dst_y_start * dst_stride;

    // ---- 3. Copy into outimage. ----
    if (fp16_int8)
    {
        // out_gpu was created with (size_t)channels elemsize / elempack=1,
        // so the downloaded data is already interleaved uint8 pixels.
        // Bounds-safe row copy: even if out.w / out.h disagree with the
        // pre-allocated out_gpu dimensions (model output mismatch, edge
        // tile, etc.) we never write past the end of outimage.
        const size_t src_row_bytes = (size_t)out.w * out.elemsize;
        tile_row_copy(dst, dst_stride, dst_capacity,
                      (const unsigned char*)out.data, src_row_bytes,
                      src_row_bytes, (size_t)out.h);
    }
    else
    {
        // out_gpu was created with float32 storage (elemsize=4u).
        // to_pixels() converts planar float → interleaved uint8 and honours
        // the destination stride so edge-tile rows land at the right place.
        const int pixel_type =
#if _WIN32
            channels == 4 ? ncnn::Mat::PIXEL_RGBA2BGRA : ncnn::Mat::PIXEL_RGB2BGR;
#else
            channels == 4 ? ncnn::Mat::PIXEL_RGBA : ncnn::Mat::PIXEL_RGB;
#endif

        const size_t packed_row_bytes = (size_t)out.w * channels;
        if (packed_row_bytes == dst_stride && (size_t)out.h * dst_stride <= dst_capacity)
        {
            // Fast path: dimensions match the output image exactly.
            out.to_pixels(dst, pixel_type, (int)dst_stride);
        }
        else
        {
            // Mismatch path (edge tile with unexpected model output size):
            // convert into a temporary tightly-packed buffer first, then
            // copy with explicit bounds clamping.
            std::vector<unsigned char> tmp(packed_row_bytes * (size_t)out.h);
            if (!tmp.empty())
            {
                out.to_pixels(tmp.data(), pixel_type);
                tile_row_copy(dst, dst_stride, dst_capacity,
                              tmp.data(), packed_row_bytes,
                              packed_row_bytes, (size_t)out.h);
            }
        }
    }
}

#endif // GPU_TILE_DOWNLOAD_H
