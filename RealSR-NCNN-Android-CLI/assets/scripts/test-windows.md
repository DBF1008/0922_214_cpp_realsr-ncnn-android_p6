# RealSR-NCNN-Android Windows 测试脚本说明

## 简介

`test-all.bat` 是用于在 Windows 平台上自动测试 RealSR-NCNN-Android CLI 工具的批处理脚本。该脚本支持测试所有可用的图像超分辨率程序，并提供彩色输出和详细的测试统计信息。

## 功能特性

- **自动测试所有程序**：支持测试 resize-ncnn、realcugan-ncnn、realsr-ncnn、srmd-ncnn、waifu2x-ncnn、mnnsr-ncnn 等
- **彩色输出显示**：使用 emoji 符号（✅/❌）和彩色文本标识测试结果
- **智能文件名生成**：输出文件名包含模型名称、缩放倍数（x2/x4等）和降噪等级（n0/n2等）
- **单程序测试模式**：可以单独测试指定程序
- **详细统计信息**：测试完成后显示参数组数、输出文件数、通过/失败测试数

## 使用方法

### 测试所有程序

```batch
test-all.bat
```

### 测试单个程序

```batch
test-all.bat <程序名>
```

支持的程序名：
- `resize-ncnn.exe`
- `realcugan-ncnn.exe`
- `realsr-ncnn.exe`
- `srmd-ncnn.exe`
- `waifu2x-ncnn.exe`
- `mnnsr-ncnn.exe`

### 显示帮助

```batch
test-all.bat help
test-all.bat --help
test-all.bat -h
```

## 输出文件名格式

输出文件名采用以下格式：

```
<模型名>_<缩放后缀>_<降噪后缀>_<输入文件名>.<扩展名>
```

### 示例

| 参数 | 输出文件名 |
|------|-----------|
| `-m bicubic -s 2` | `bicubic_x2_pixel.png` |
| `-m models-cunet -n 0` | `models-cunet_n0_pixel.png` |
| `-m models-cunet -n 2` | `models-cunet_n2_pixel.png` |
| `-m ESRGAN-MoeSR-jp_Illustration-x4.mnn -s 4` | `ESRGAN-MoeSR-jp_Illustration-x4.mnn_x4_pixel.png` |

### 命名规则

- **模型名**：提取 `-m` 参数的值，去除文件夹路径
- **缩放后缀**：`-s` 参数值前加 `x`，如 `x2`、`x4`、`x0.5`
- **降噪后缀**：`-n` 参数值前加 `n`，如 `n0`、`n2`

## 目录结构

```
RealSR-NCNN-Android-CLI/
├── assets/
│   ├── input/          # 输入图片目录
│   ├── output/         # 输出结果目录
│   └── scripts/
│       └── test-all.bat    # 测试脚本
├── Win-x64/            # 程序目录
│   ├── resize-ncnn.exe
│   ├── realcugan-ncnn.exe
│   ├── realsr-ncnn.exe
│   ├── srmd-ncnn.exe
│   ├── waifu2x-ncnn.exe
│   └── mnnsr-ncnn.exe
```

## 测试参数配置

### resize-ncnn.exe

| 参数组 | 参数 | 说明 |
|--------|------|------|
| 1 | `-m bicubic -s 2` | 双三次插值，放大2倍 |
| 2 | `-m bilinear -s 2` | 双线性插值，放大2倍 |
| 3 | `-m nearest -s 2` | 最近邻插值，放大2倍 |
| 4 | `-m avir -s 2` | AVIR算法，放大2倍 |
| 5 | `-m avir-lancir -s 2` | AVIR-Lancir算法，放大2倍 |
| 6 | `-m avir -s 0.5` | AVIR算法，缩小到0.5倍 |
| 7 | `-m de-nearest -s 2` | 反最近邻插值，放大2倍 |
| 8 | `-m de-nearest2 -s 2` | 反最近邻插值2，放大2倍 |
| 9 | `-m de-nearest3 -s 2` | 反最近邻插值3，放大2倍 |

### realcugan-ncnn.exe

| 参数组 | 参数 | 说明 |
|--------|------|------|
| 1 | `-m models-nose -s 2 -n 0` | 无SE模型，放大2倍，无降噪 |
| 2 | `-m models-se -s 2 -n -1` | SE模型，放大2倍，保守降噪 |
| 3 | `-m models-se -s 2 -n 0` | SE模型，放大2倍，无降噪 |
| 4 | `-m models-pro -s 2 -n -1` | Pro模型，放大2倍，保守降噪 |
| 5 | `-m models-pro -s 2 -n 0` | Pro模型，放大2倍，无降噪 |

### realsr-ncnn.exe

| 参数组 | 参数 | 说明 |
|--------|------|------|
| 1 | `-m models-Real-ESRGAN-anime` | Real-ESRGAN动漫模型 |
| 2 | `-m models-Real-ESRGANv3-general -s 4` | Real-ESRGANv3通用模型，放大4倍 |
| 3 | `-m models-Real-ESRGANv3-anime -s 3` | Real-ESRGANv3动漫模型，放大3倍 |

### srmd-ncnn.exe

| 参数组 | 参数 | 说明 |
|--------|------|------|
| 1 | `-s 2` | 放大2倍 |
| 2 | `-s 3` | 放大3倍 |
| 3 | `-s 4` | 放大4倍 |

### waifu2x-ncnn.exe

| 参数组 | 参数 | 说明 |
|--------|------|------|
| 1 | `-m models-cunet -n 0` | CUNet模型，无降噪 |
| 2 | `-m models-cunet -n 2` | CUNet模型，降噪等级2 |
| 3 | `-m models-upconv_7_anime_style_art_rgb -n 0` | Upconv7动漫模型，无降噪 |
| 4 | `-m models-upconv_7_anime_style_art_rgb -n 2` | Upconv7动漫模型，降噪等级2 |
| 5 | `-m models-upconv_7_photo -n 0` | Upconv7照片模型，无降噪 |
| 6 | `-m models-upconv_7_photo -n 2` | Upconv7照片模型，降噪等级2 |

### mnnsr-ncnn.exe

| 参数组 | 参数 | 说明 |
|--------|------|------|
| 1 | `-m models-MNN/ESRGAN-MoeSR-jp_Illustration-x4.mnn -s 4` | MNN动漫插画模型，放大4倍 |
| 2 | `-m models-MNN/ESRGAN-Nomos8kSC-x4.mnn -s 4` | MNN Nomos8kSC模型，放大4倍 |

## 输出示例

```
Testing program: resize-ncnn.exe
Test parameter group 1: -m bicubic -s 2
Processing file: pixel.png
...
✅ [OK] Test succeeded: bicubic_x2_pixel.png

Test parameter group 2: -m bilinear -s 2
Processing file: pixel.png
...
✅ [OK] Test succeeded: bilinear_x2_pixel.png

...

All tests completed!
Results saved in A:\...\output

Test Statistics:
Total parameter groups tested: 28
Total output files generated: 25
✅ Total passed tests: 25
❌ Total failed tests: 3
```

## 注意事项

1. **输入文件**：将待测试的图片放入 `assets/input/` 目录
2. **输出文件**：测试结果保存在 `assets/output/` 目录
3. **程序路径**：确保所有可执行文件位于 `Win-x64/` 目录
4. **模型文件**：部分程序需要相应的模型文件才能正常运行
5. **运行环境**：需要 Windows 系统和 PowerShell 支持彩色输出

## 故障排除

### 找不到程序

确保程序文件存在于 `Win-x64/` 目录中，并且文件名正确。

### 模型加载失败

检查模型文件是否存在于正确的位置，路径是否正确。

### 输出文件为空

某些参数组合可能不适用于所有输入图片，这是正常现象。脚本会记录失败但继续测试其他参数组。

## 技术细节

- 使用 `setlocal enabledelayedexpansion` 支持变量延迟扩展
- 使用 PowerShell 实现彩色输出（`Write-Host -ForegroundColor`）
- 使用 Unicode emoji 字符（✅ U+2705, ❌ U+274C）标识结果
- 自动清理输出目录，避免文件冲突
