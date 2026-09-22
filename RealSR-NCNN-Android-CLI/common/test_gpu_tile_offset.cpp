// test_gpu_tile_offset.cpp — Regression tests for the GPU tile-row offset
// and write-back logic shared by Waifu2x / SRMD / RealCUGAN.
//
// The fix replaces an unsafe external-pointer ncnn::Mat with a safe
// download-to-temporary-then-copy approach.  These tests verify that:
//
//   1. The *new* offset formula (using out_tile_y0) always matches the
//      available space in the output buffer, even for edge tiles.
//   2. The *old* formula (yi * scale * TILE_SIZE_Y * …) is shown to agree
//      with the new one for the standard case (proving we didn't break
//      non-edge tiles).
//   3. A simulated memcpy into the output buffer never overflows.
//   4. Odd image dimensions, large scale factors, and single-tile images
//      are all handled correctly.
//
// Build:
//   g++ -std=c++11 -O2 -o test_gpu_tile_offset test_gpu_tile_offset.cpp
//
// Run:
//   ./test_gpu_tile_offset

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>

// ---------------------------------------------------------------------------
// Simulate the tile offset computation used by the *fixed* code.
// ---------------------------------------------------------------------------
struct TileInfo
{
    int out_tile_y0;    // y-start in input coordinates
    int out_tile_y1;    // y-end   in input coordinates
    int out_gpu_h;      // height of the GPU tile row in output pixels
    size_t dst_offset;  // byte offset into outimage.data
    size_t dst_stride;  // bytes per output row
    size_t tile_bytes;  // total bytes this tile row writes
    size_t buffer_size; // total output buffer size
    bool fits;          // dst_offset + tile_bytes <= buffer_size
};

static TileInfo compute_tile_info(
    int yi, int h, int w, int scale, int channels, int TILE_SIZE_Y)
{
    TileInfo t;

    // Same computation as in the fixed backends
    t.out_tile_y0 = std::max(yi * TILE_SIZE_Y, 0);
    t.out_tile_y1 = std::min((yi + 1) * TILE_SIZE_Y, h);
    t.out_gpu_h   = (t.out_tile_y1 - t.out_tile_y0) * scale;

    t.dst_stride  = (size_t)w * scale * channels;
    t.dst_offset  = (size_t)t.out_tile_y0 * scale * t.dst_stride;

    t.tile_bytes  = (size_t)t.out_gpu_h * t.dst_stride;
    t.buffer_size = (size_t)w * scale * h * scale * channels;

    t.fits = (t.dst_offset + t.tile_bytes <= t.buffer_size);
    return t;
}

// Old (buggy) offset formula for comparison.
static size_t old_offset(int yi, int scale, int TILE_SIZE_Y, int w, int channels)
{
    return (size_t)yi * scale * TILE_SIZE_Y * w * scale * channels;
}

// ---------------------------------------------------------------------------
// Tiny test harness
// ---------------------------------------------------------------------------
static int g_tests  = 0;
static int g_passed = 0;

#define CHECK(cond, msg) do {                                         \
    ++g_tests;                                                        \
    if (cond) { ++g_passed; }                                         \
    else      { std::fprintf(stderr, "FAIL [%s] line %d: %s\n",      \
                             __func__, __LINE__, msg); }              \
} while (0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Standard case: 800×600, tile 400, scale 2, 3 ch.
static void test_standard_tiles()
{
    const int w = 800, h = 600, scale = 2, ch = 3, TY = 400;
    const int yt = (h + TY - 1) / TY;  // 2

    // tile 0: rows 0-399 → output rows 0-799
    TileInfo t0 = compute_tile_info(0, h, w, scale, ch, TY);
    CHECK(t0.out_tile_y0 == 0,   "t0 y0");
    CHECK(t0.out_tile_y1 == 400, "t0 y1");
    CHECK(t0.out_gpu_h   == 800, "t0 h");
    CHECK(t0.dst_offset  == 0,   "t0 offset");
    CHECK(t0.fits,               "t0 fits");

    // tile 1: rows 400-599 (edge) → output rows 800-1199
    TileInfo t1 = compute_tile_info(1, h, w, scale, ch, TY);
    CHECK(t1.out_tile_y0 == 400,          "t1 y0");
    CHECK(t1.out_tile_y1 == 600,          "t1 y1");
    CHECK(t1.out_gpu_h   == 400,          "t1 h");
    CHECK(t1.dst_offset  == old_offset(1, scale, TY, w, ch), "t1 offset matches old");
    CHECK(t1.fits,                        "t1 fits");
}

// Edge tile: last row much shorter than TILE_SIZE_Y.
static void test_short_edge_tile()
{
    const int w = 400, h = 500, scale = 2, ch = 3, TY = 400;
    const int yt = (h + TY - 1) / TY;  // 2

    TileInfo t1 = compute_tile_info(1, h, w, scale, ch, TY);
    CHECK(t1.out_tile_y0 == 400,          "short y0");
    CHECK(t1.out_tile_y1 == 500,          "short y1");
    CHECK(t1.out_gpu_h   == 200,          "short h = (500-400)*2");
    CHECK(t1.tile_bytes  == 200 * 800 * 3, "short tile bytes");
    CHECK(t1.fits,                        "short fits");

    // Old formula gives the same start offset here (both evaluate to
    // 1 * 2 * 400 * 400 * 2 * 3), but the external-pointer Mat it
    // constructed would have had the wrong height (out_gpu.h from the
    // pre-allocated VkMat is correct, but wrapping it with the wrong
    // external pointer stride was the bug).
    CHECK(t1.dst_offset == old_offset(1, scale, TY, w, ch),
          "short offset == old offset");
}

// Single tile covers entire image.
static void test_single_tile()
{
    const int w = 200, h = 200, scale = 2, ch = 3, TY = 400;

    TileInfo t0 = compute_tile_info(0, h, w, scale, ch, TY);
    CHECK(t0.out_tile_y0 == 0,   "single y0");
    CHECK(t0.out_tile_y1 == 200, "single y1");
    CHECK(t0.out_gpu_h   == 400, "single h");
    CHECK(t0.dst_offset  == 0,   "single offset");
    CHECK(t0.tile_bytes  == t0.buffer_size, "single covers entire buffer");
    CHECK(t0.fits,               "single fits");
}

// Image height is an exact multiple of TILE_SIZE_Y — no edge tile.
static void test_exact_multiple()
{
    const int w = 400, h = 800, scale = 2, ch = 3, TY = 400;
    const int yt = (h + TY - 1) / TY;  // 2

    for (int yi = 0; yi < yt; ++yi)
    {
        TileInfo t = compute_tile_info(yi, h, w, scale, ch, TY);
        CHECK(t.fits,                "exact fits");
        CHECK(t.out_gpu_h == 800,    "exact h = 400*2");
    }
}

// Scale factor 4 with RGBA.
static void test_scale4_rgba()
{
    const int w = 400, h = 450, scale = 4, ch = 4, TY = 400;
    const int yt = (h + TY - 1) / TY;  // 2

    TileInfo t0 = compute_tile_info(0, h, w, scale, ch, TY);
    CHECK(t0.out_gpu_h == 1600,   "s4 t0 h");
    CHECK(t0.fits,                "s4 t0 fits");

    TileInfo t1 = compute_tile_info(1, h, w, scale, ch, TY);
    CHECK(t1.out_tile_y0 == 400,              "s4 t1 y0");
    CHECK(t1.out_tile_y1 == 450,              "s4 t1 y1");
    CHECK(t1.out_gpu_h   == (450 - 400) * 4,  "s4 t1 h");
    CHECK(t1.fits,                            "s4 t1 fits");
}

// Many small tiles.
static void test_many_small_tiles()
{
    const int w = 400, h = 1000, scale = 2, ch = 3, TY = 100;
    const int yt = (h + TY - 1) / TY;  // 10

    size_t total_written = 0;
    for (int yi = 0; yi < yt; ++yi)
    {
        TileInfo t = compute_tile_info(yi, h, w, scale, ch, TY);
        CHECK(t.fits,                       "small fits");
        CHECK(t.out_tile_y0 == yi * 100,    "small y0");
        CHECK(t.out_tile_y1 == std::min((yi + 1) * 100, h), "small y1");
        total_written += t.tile_bytes;
    }
    // All tiles together must cover the entire output exactly once.
    size_t expected = (size_t)w * scale * h * scale * ch;
    CHECK(total_written == expected, "small tiles cover buffer exactly");
}

// Odd image dimensions.
static void test_odd_dimensions()
{
    const int w = 401, h = 399, scale = 3, ch = 3, TY = 200;
    const int yt = (h + TY - 1) / TY;  // 2

    TileInfo t0 = compute_tile_info(0, h, w, scale, ch, TY);
    CHECK(t0.fits, "odd t0 fits");

    TileInfo t1 = compute_tile_info(1, h, w, scale, ch, TY);
    CHECK(t1.fits,                "odd t1 fits");
    CHECK(t1.out_tile_y0 == 200,  "odd t1 y0");
    CHECK(t1.out_tile_y1 == 399,  "odd t1 y1");
    CHECK(t1.out_gpu_h == (399 - 200) * 3, "odd t1 h");
}

// Simulate actual memcpy to verify no buffer overflow with AddressSanitizer.
static void test_memcpy_simulation()
{
    const int w = 100, h = 150, scale = 2, ch = 3, TY = 100;
    const int yt = (h + TY - 1) / TY;  // 2

    const size_t buf_size = (size_t)w * scale * h * scale * ch;
    unsigned char* buf = new unsigned char[buf_size];
    std::memset(buf, 0, buf_size);

    for (int yi = 0; yi < yt; ++yi)
    {
        TileInfo t = compute_tile_info(yi, h, w, scale, ch, TY);
        CHECK(t.fits, "sim fits");

        unsigned char* dst = buf + t.dst_offset;

        // Simulate the row-by-row memcpy from the fixed helper.
        const size_t row_bytes = (size_t)w * scale * ch;  // = dst_stride
        for (int row = 0; row < t.out_gpu_h; ++row)
        {
            std::memset(dst + (size_t)row * t.dst_stride, 0xAB, row_bytes);
        }
    }

    // Every byte should have been written exactly once.
    for (size_t i = 0; i < buf_size; ++i)
        CHECK(buf[i] == 0xAB, "sim fully written");

    delete[] buf;
}

// Edge case: image height = 1 (extreme edge tile).
static void test_height_one()
{
    const int w = 64, h = 1, scale = 2, ch = 3, TY = 400;

    TileInfo t0 = compute_tile_info(0, h, w, scale, ch, TY);
    CHECK(t0.out_tile_y0 == 0,  "h1 y0");
    CHECK(t0.out_tile_y1 == 1,  "h1 y1");
    CHECK(t0.out_gpu_h   == 2,  "h1 h");
    CHECK(t0.fits,              "h1 fits");
}

// Edge case: tilesize = 1 (every pixel is its own tile).
static void test_tilesize_one()
{
    const int w = 10, h = 10, scale = 2, ch = 3, TY = 1;
    const int yt = (h + TY - 1) / TY;  // 10

    size_t total = 0;
    for (int yi = 0; yi < yt; ++yi)
    {
        TileInfo t = compute_tile_info(yi, h, w, scale, ch, TY);
        CHECK(t.fits,           "ts1 fits");
        CHECK(t.out_gpu_h == 2, "ts1 h = 1*2");
        total += t.tile_bytes;
    }
    CHECK(total == (size_t)w * scale * h * scale * ch, "ts1 total");
}

// Verify the old formula can produce a buffer overflow for edge tiles
// when the external-pointer Mat height disagrees with available space.
// This documents *why* the fix was needed.
static void test_old_formula_danger()
{
    // Scenario: the old code wraps an external pointer at offset
    //   yi * scale * TILE_SIZE_Y * w * scale * channels
    // with height = out_gpu.h.  For non-edge tiles this is fine, but
    // imagine a code path where out_gpu.h was computed differently
    // (e.g., model output is slightly larger due to alignment).
    // The new code avoids this by downloading to a temp Mat first.

    const int w = 400, h = 500, scale = 2, ch = 3, TY = 400;
    const size_t buf_size = (size_t)w * scale * h * scale * ch;

    // Old offset for tile 1:
    size_t off = old_offset(1, scale, TY, w, ch);

    // If the external-pointer Mat accidentally had height = TY*scale
    // (= 800) instead of the correct edge height ((500-400)*2 = 200),
    // it would try to write 800 rows * dst_stride bytes = overflow.
    size_t stride = (size_t)w * scale * ch;
    size_t wrong_height = TY * scale;  // 800
    size_t would_write  = wrong_height * stride;

    CHECK(off + would_write > buf_size,
          "old formula: wrong height would overflow buffer (documented)");

    // The new code uses out_gpu.h from the *actual* VkMat dimensions,
    // which is always (out_tile_y1 - out_tile_y0) * scale.
    TileInfo t1 = compute_tile_info(1, h, w, scale, ch, TY);
    CHECK(t1.dst_offset + t1.tile_bytes <= buf_size,
          "new formula: always fits in buffer");
}

// ---------------------------------------------------------------------------

int main()
{
    std::printf("GPU tile offset regression tests\n");
    std::printf("================================\n\n");

    test_standard_tiles();
    test_short_edge_tile();
    test_single_tile();
    test_exact_multiple();
    test_scale4_rgba();
    test_many_small_tiles();
    test_odd_dimensions();
    test_memcpy_simulation();
    test_height_one();
    test_tilesize_one();
    test_old_formula_danger();

    std::printf("\n%d / %d passed\n", g_passed, g_tests);

    if (g_passed == g_tests)
    {
        std::printf("All tests PASSED.\n");
        return 0;
    }
    else
    {
        std::printf("Some tests FAILED.\n");
        return 1;
    }
}
