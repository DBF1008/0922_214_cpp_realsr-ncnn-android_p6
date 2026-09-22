# 图像一致性评估报告 - 脚本说明

## 概述

本系统采用**数据与视图分离**的架构，通过Python脚本生成评估数据，HTML文件作为独立查看器。本目录包含生成报告的核心脚本和相关工具。

## 目录结构

```
assets/scripts/
├── evaluate_image_consistency.py    # 生成JSON和HTML报告的核心脚本
├── report_viewer.html              # 查看器模板
├── report_viewer2.html             # 增强版查看器（支持漂移放大效果）
├── drift-zoom-demo.html            # drift 放大效果演示
├── test-all.bat                    # 测试所有模型的批处理文件
├── test-all-new.bat                # 测试所有模型的批处理文件（新版）
├── test-realsr.bat                 # 测试 RealSR 模型的批处理文件
├── test-realcugan.bat              # 测试 RealCUGAN 模型的批处理文件
├── test-srmd.bat                   # 测试 SRMD 模型的批处理文件
├── test-waifu2x.bat                # 测试 Waifu2x 模型的批处理文件
├── test-mnnsr.bat                  # 测试 MNNSR 模型的批处理文件
├── test-resize.bat                 # 测试调整大小的批处理文件
├── test-windows.md                 # Windows 测试说明
└── waifu2xreference.cpp            # Waifu2x 参考代码
```

## 使用方法

### 生成评估报告

运行Python脚本生成JSON和HTML报告：

```bash
cd assets/scripts
python evaluate_image_consistency.py "your_csv_file.csv"
```

这会生成：
- `../report/data/your_csv_file.json` - JSON数据文件
- `../report/your_csv_file.html` - HTML报告（内嵌样式）

### 创建查看器

复制查看器模板：

```bash
cd assets/scripts
copy report_viewer.html ../report/your_report_viewer.html
```

### 测试模型

使用批处理文件测试不同模型：

```bash
cd assets/scripts
# 测试所有模型
./test-all.bat

# 测试特定模型
./test-realsr.bat
./test-realcugan.bat
./test-srmd.bat
./test-waifu2x.bat
./test-mnnsr.bat
./test-resize.bat
```

## 功能说明

### evaluate_image_consistency.py

核心功能：
- 读取CSV文件，分析图像一致性
- 计算透明度异常、RGB异常等指标
- 生成JSON数据文件
- 创建HTML报告
- 支持批处理模式

### 查看器功能

#### report_viewer.html（基础版）
- 自动加载JS数据
- 统计摘要显示
- 高级筛选功能
- 表格排序和导出
- 图像查看器

#### report_viewer2.html（增强版）
- 基础版所有功能
- 支持 drift 放大效果
- 并排显示图片和详情
- 表格形式的详情数据
- 背景色切换

### 测试批处理文件

- **test-all.bat**：测试所有模型
- **test-realsr.bat**：测试 RealSR 模型
- **test-realcugan.bat**：测试 RealCUGAN 模型
- **test-srmd.bat**：测试 SRMD 模型
- **test-waifu2x.bat**：测试 Waifu2x 模型
- **test-mnnsr.bat**：测试 MNNSR 模型
- **test-resize.bat**：测试调整大小

## 技术架构

### 数据流程

1. **输入**：CSV文件，包含测试结果
2. **处理**：Python脚本分析图像一致性
3. **输出**：JSON数据文件和HTML报告
4. **查看**：HTML查看器加载并显示数据

### 数据格式

#### JSON文件
```json
{
  "metadata": {
    "timestamp": "20260205_021852",
    "total_files": 18,
    "csv_file": "test_resize_20260205_021852.csv"
  },
  "severity_counts": {
    "normal": 12,
    "mild": 5,
    "moderate": 1,
    "severe": 0
  },
  "csv_stats": {
    "total_csv": 20,
    "matched": 18,
    "unmatched": 0,
    "empty_output": 2
  },
  "results": [...]
}
```

#### JS文件（推荐使用）
```javascript
window.reportData = {
  "metadata": {...},
  "severity_counts": {...},
  "csv_stats": {...},
  "results": [...]
};
```

## 增强版查看器特性

### drift 放大效果

- 支持鼠标悬停放大
- 平滑的放大过渡效果
- 可自定义放大倍数
- 支持触摸事件

### 并排显示

- 图片和详情并排显示
- 响应式布局
- 可调整的宽度比例

### 背景色切换

- 支持灰色、黑色、白色、棋盘格背景
- 背景色同步到放大面板
- 表格配色自动适配

## 优势

### 数据与视图分离
- Python只负责生成JSON数据
- HTML只负责展示数据
- 修改样式不影响数据生成

### 灵活使用
- 支持多种查看方式
- 可扩展性强
- 支持自定义配置

### 易于维护
- 模块化设计
- 清晰的文件结构
- 详细的说明文档

## 联系方式

如有问题，请查看项目文档或联系开发团队。
