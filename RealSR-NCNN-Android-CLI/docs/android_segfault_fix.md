# Android端 Segmentation Fault 修复

## 问题描述

- **平台**: Android (Adreno 740 GPU)
- **现象**: 处理图像时在最后一个 tile 行发生 Segmentation fault (exit code 139)
- **条件**: 启用 `fp16_storage`/`fp16_packed` + `int8_storage` 时触发
- **Windows端**: 正常，不触发

## 崩溃日志

```
yi=6/8 tile_h_nopad=64 in_tile_y0=374 in_tile_y1=456
yi=6 ncnn::Mat in created: 342x82
yi=6 VkCompute cmd created
yi=6 record_clone start
Segmentation fault
```

- 输入图像: 342x456, channels=1
- tilesize=64, prepadding=10, scale=4
- 崩溃位置: `yi=6` (第7行tile), `record_clone` 阶段

## 根因分析

当 `yi=6` 时:
- `in_tile_y0 = 374`, `in_tile_y1 = min(458, 456) = 456`
- `safe_tile_h = 456 - 374 = 82`（非标准大小，标准应为 `64 + 20 = 84`）

FP16+INT8 路径直接使用外部指针构造 `ncnn::Mat`:
```cpp
// 原始代码 — 对非标准 tile 高度，GPU 内存对齐失败
in = ncnn::Mat(w, 82, (unsigned char*)pixeldata + 374 * w * channels, (size_t)channels, 1);
```

Android GPU 在 FP16 模式下对非标准行数的外部指针 Mat 执行 `record_clone` 时内存对齐异常，导致段错误。

## 修复方案

核心思路: 对非标准尺寸的 tile strip，使用 `memcpy` 创建独立内存的 Mat，避免外部指针 Mat 在 GPU 上传时因内存对齐失败而崩溃。标准尺寸的 tile 仍保持零拷贝路径以兼顾性能。

## 修复总览

| 模块 | 模式1 (输入 tile 上传) | 模式2a (输出 tile 下载) | 状态 |
|------|----------------------|------------------------|------|
| `RealSR/src/main/jni/realsr.cpp` | 行 ~248 | 行 ~585 | ✅ 已修复 |
| `Waifu2x/src/main/jni/waifu2x.cpp` | 行 ~185 | 行 ~475 | ✅ 已修复 |
| `SRMD/src/main/jni/srmd.cpp` | 行 ~235 | 行 ~558 | ✅ 已修复 |
| `RealCUGAN/src/main/jni/realcugan.cpp` | 行 321, 1417, 1704, 2329 (4处) | 行 725, 2097 (2处) | ✅ 已修复 |

### 模式1: 输入 tile — 非标准尺寸使用 memcpy

**RealSR** (标准尺寸 = `TILE_SIZE_Y + 2 * prepadding`):
```cpp
if ((opt.use_fp16_storage || opt.use_fp16_packed) && opt.use_int8_storage)
{
    const int safe_tile_h = in_tile_y1 - in_tile_y0;
    const bool is_standard_size = (safe_tile_h == TILE_SIZE_Y + 2 * prepadding);
    if (is_standard_size) {
        // 标准tile: 零拷贝外部指针（性能优先）
        in = ncnn::Mat(w, safe_tile_h, (unsigned char*)pixeldata + in_tile_y0 * w * channels, (size_t)channels, 1);
    } else {
        // 边界tile: memcpy确保内存对齐（安全优先）
        in.create(w, safe_tile_h, (size_t)channels, 1);
        const unsigned char* src = (unsigned char*)pixeldata + in_tile_y0 * w * channels;
        memcpy(in.data, src, (size_t)w * safe_tile_h * channels);
    }
}
```

**Waifu2x / SRMD** — 同上模式，条件为 `opt.use_fp16_storage && opt.use_int8_storage`。

**RealCUGAN** — 标准尺寸判断使用 `TILE_SIZE_Y + prepadding + prepadding_bottom`（因 `prepadding_bottom` 根据 `scale` 和实际 tile 高度做额外对齐填充，与 `prepadding` 不同）。

### 模式2a: 输出 tile 下载 offset

download 阶段同样使用外部指针 Mat + `record_clone`，非标准输出 tile 高度可能存在相同风险。
四个后端现已统一改为 `common/gpu_tile_download.h` 中的
`download_gpu_tile_to_output()` 安全实现：先下载到 ncnn 持有的临时 CPU Mat，
再按实际 tile 边界 (`out_tile_y0`) 写回 `outimage`，写回过程经
`common/tile_row_copy.h` 的 `tile_row_copy()` 做边界钳制，绝不越界。

```cpp
// 修复前（外部指针 + 固定公式，边缘 tile 可能越界/错位）
out = ncnn::Mat(out_gpu.w, out_gpu.h,
    (unsigned char*)outimage.data + yi * scale * TILE_SIZE_Y * w * scale * channels,
    (size_t)channels, 1);

// 修复后（统一下载到临时 Mat，再按实际 tile 边界安全写回）
download_gpu_tile_to_output(
    out_gpu, outimage, out_tile_y0, scale, w, channels,
    (opt.use_fp16_storage && opt.use_int8_storage),
    cmd, opt);
```

安全保证：
- 不再为 `outimage` 构造外部指针 Mat，Android 上 `record_clone` 不会
  直接写用户缓冲区；
- 写回偏移使用调用方已算好的真实 tile 边界 `out_tile_y0`，而非固定公式；
- `tile_row_copy()` 对行宽与行数按 `outimage` 剩余容量钳制，模型输出
  尺寸与预期不一致时也不会越界写；
- float 路径尺寸不匹配时先 `to_pixels` 到临时紧凑缓冲，再钳制拷贝。

回归验证：`common/test_gpu_tile_offset.cpp`（偏移公式 + `tile_row_copy`
钳制共 11 组用例），仓库根目录 `test.sh` 一键编译运行，支持 `--asan`。

### 模式2b: `to_pixels` 路径

`to_pixels` 不涉及外部指针 Mat 构造（内部会自行拷贝），风险较低。
