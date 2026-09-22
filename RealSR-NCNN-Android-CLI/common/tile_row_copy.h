// tile_row_copy.h — Bounds-safe interleaved row copy for tile write-back.
//
// This is the pure (ncnn-free) core of the GPU tile download fix in
// gpu_tile_download.h.  It is kept in a separate header so the unit tests
// (test_gpu_tile_offset.cpp) can exercise the clamping logic without
// pulling in ncnn / Vulkan headers.
//
// Contract
// --------
// Copy `rows` rows of `row_bytes` bytes from a tightly-strided source
// into a strided destination, never writing past `dst_capacity` bytes
// and never reading more than `row_bytes` from any source row.
//
//   • If `row_bytes` exceeds `dst_stride` (source row wider than the
//     destination row, e.g. model output width mismatch), the copy is
//     clipped to `dst_stride` bytes per row.
//   • If `rows` exceeds the number of full rows that fit in
//     `dst_capacity`, the copy is clipped to the rows that fit.
//
// Returns the number of rows actually written.

#ifndef TILE_ROW_COPY_H
#define TILE_ROW_COPY_H

#include <cstring>
#include <cstddef>

static inline size_t tile_row_copy(
    unsigned char* dst, size_t dst_stride, size_t dst_capacity,
    const unsigned char* src, size_t src_stride,
    size_t row_bytes, size_t rows)
{
    if (!dst || !src || dst_stride == 0 || row_bytes == 0 || rows == 0)
        return 0;

    // Clip the per-row copy width to what a destination row can hold.
    if (row_bytes > dst_stride)
        row_bytes = dst_stride;

    // Clip the row count to the rows that fully fit in the destination.
    const size_t max_rows = dst_capacity / dst_stride;
    if (rows > max_rows)
        rows = max_rows;

    for (size_t r = 0; r < rows; ++r)
    {
        std::memcpy(dst + r * dst_stride,
                    src + r * src_stride,
                    row_bytes);
    }

    return rows;
}

#endif // TILE_ROW_COPY_H
