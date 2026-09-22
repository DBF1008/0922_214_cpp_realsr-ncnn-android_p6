[English](./README.md)

# MNN-SR

这个模块是用 [mnn](https://github.com/alibaba/MNN) 实现的超分辨率命令行程序。有如下特色:  
1. mnn可以比ncnn兼容更多模型架构。  
2. 本模块支持gcc、VS和AS编译，因此不止是Android，连Windows和Linux甚至其他PC系统也有脱离Python的更为通用的超分辨率工具了！  
3. mnn支持多种后端, 当前配置Android端支持vulkan、opencl和cpu,推荐opencl。Windows端支持vulkan、opencl、CPU和cuda，其中cuda在我的电脑中输出结果错误，需要更多测试和反馈；opencl在系统任务管理器中看不到gpu占用，在nvidia-smi工具中可以看到gpu能够跑满。Linux支持vulkan、opencl、CPU和cuda，我的测试环境中cuda最快，vulkan无法调用到gpu，opencl表现良好。整体来看，各平台都能正常使用并且速度不低于ncnn。  
**如果可能的话，请你帮我进一步完善🙏！**

### 如何在AS中编译

1. 和前边的ncnn模块一样下载并解压依赖到 RealSR-NCNN-Android/3rdparty  

2. 下载mnn库并解压到RealSR-NCNN-Android/3rdparty
   
   ```
   ├─libwebp
   ├─mnn_android
   │  ├─arm64-v8a
   │  ├─armeabi-v7a
   │  └─include
   ```

3. sync 并 build

4. 从mnn库中复制 *.so 文件到GUI项目的assest中. 

### 如何在VS中编译Windows x64

1. 和Android版本一样下载Windows的各项依赖，注意如果需要cuda加速，需要重新编译mnn （然而即使编译成功，也不见得可以获得正确的推理结果）
编译mnn的命令参考
```
mkdir build && cd build

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DMNN_BUILD_SHARED_LIBS=ON -DMNN_WIN_RUNTIME_MT=ON -DMNN_CUDA=ON  -DMNN_OPENCL=ON -DMNN_VULKAN=ON  -DCUDA_CUDART_LIBRARY="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8/lib/x64/cudart.lib"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DMNN_BUILD_SHARED_LIBS=ON -DMNN_WIN_RUNTIME_MT=ON -DMNN_CUDA=ON  -DMNN_OPENCL=ON -DMNN_VULKAN=ON  -DCUDA_CUDART_LIBRARY="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8/lib/x64/cudart_static.lib"

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DMNN_BUILD_SHARED_LIBS=ON -DMNN_WIN_RUNTIME_MT=ON -DMNN_CUDA=ON -DMNN_TENSORRT=ON  -DMNN_OPENCL=ON -DMNN_VULKAN=ON  -DCUDA_CUDART_LIBRARY="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8/lib/x64/cudart.lib" 

cmake .. -G Ninja  -DCMAKE_CUDA_STANDARD=17 -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release -DMNN_BUILD_SHARED_LIBS=ON -DMNN_WIN_RUNTIME_MT=ON -DMNN_CUDA=ON -DMNN_TENSORRT=ON  -DMNN_OPENCL=ON -DMNN_VULKAN=ON -DMNN_SUPPORT_TRANSFORMER_FUSE=ON  -DCUDA_CUDART_LIBRARY="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8/lib/x64/cudart.lib" 


ninja
```

2. 根据实际路径调整CMake中的文件路径

3. 使用VS打开MNNSR的jni目录，刷新CMake文件

4. build

### 如何使用gcc编译Linux x64
请参考ci脚本进行编译  

### 用法

和realsr-ncnn基本相同，增加了如下参数：

```console
  -b backend           推理后端类型（需要注意的是，这只是设置的后端类型，实际调用时mnn框架可能会自动调整，请留意程序运行时打印的信息）(CPU=0,AUTO=4,CUDA=2,OPENCL=3,OPENGL=6,VULKAN=7,NN=5,USER_0=8,USER_1=9, default=3)
  -c color-type        模型和输出图片的色彩空间(RGB=1, BGR=2, YCbCr=5, YUV=6, GRAY=10, GRAY模型+YCbCr色彩转换=11, GRAY模型+YUV色彩转换=12, default=1)
  -d decensor-mode     去审核模式，使用此模式则输出的图片与输入图片的分辨率相同(关闭=-1, 去马赛克=0, default=-1)
```


# NCNN 各模块

## 如何编译 RealSR-NCNN-Android-CLI

### step1

https://github.com/Tencent/ncnn/releases  
下载 `ncnn-yyyymmdd-android-vulkan-shared.zip` 或者你自己编译ncnn为so文件  
https://github.com/webmproject/libwebp  
下载libwebp的源码
https://opencv.org/releases/  
下载opencv-android-sdk

### step2

解压 `ncnn-yyyymmdd-android-vulkan-shared.zip` 到 `../3rdparty/ncnn-android-vulkan-shared`  
解压libwebp源码到`../3rdparty/libwebp`  
解压 `opencv-version-android-sdk` 到 `../3rdparty/opencv-android-sdk`

```
RealSR-NCNN-Android
├─3rdparty

│   ├─opencv-android-sdk
│   │   └─sdk
│   ├─libwebp
│   └─ncnn-android-vulkan-shared
│       └─arm64-v8a
├─RealSR-NCNN-Android-CLI
│   ├─Anime4k
│   ├─RealCUGAN
│   ├─Waifu2x
│   ├─RealSR
│   ├─SRMD
│   └─ReSize
└─RealSR-NCNN-Android-GUI
```

### step3

用 Android Studio 打开工程, rebuild 然后你就可以在 `RealSR-NCNN-Android-CLI\*\build\intermediates\cmake\release\obj\arm64-v8a` 或 `RealSR-NCNN-Android-CLI\*\build\intermediates\cmake\debug\obj\arm64-v8a` 找到编译好的二进制文件，这些文件会被编译脚本自动复制到 GUI 工程目录中。  
点击 `3rdparty/copy_cli_build_result.bat` 可以更新其他库文件的二进制文件到 GUI 工程目录中

## 如何使用 RealSR-NCNN-Android-CLI

### 下载模型

你可以从 github release 页面下载 `assets.zip`, 或者从 https://github.com/tumuyan/realsr-models 下载所需模型，需要注意不同程序需要用对应的模型

### 命令范例

确认程序有执行权限，然后输入命令：

```shell
./realsr-ncnn -i input.jpg -o output.jpg
```

### 完整用法

仅以 realsr-ncnn 为例说明，其他程序使用方法完全相同，故不重复说明

```console
用法: realsr-ncnn -i 输入的图片路径 -o 输出的图片路径 [其他可选参数]...

  -h                   显示帮助
  -v                   显示更多输出内容
  -i input-path        输入的图片路径（jpg/png/webp路径或者目录路径）
  -o output-path       输出的图片路径（jpg/png/webp路径或者目录路径）
  -s scale             缩放系数(默认4，即放大4倍)
  -t tile-size         tile size (>=32/0=auto, default=0) can be 0,0,0 for multi-gpu
  -m model-path        模型路径 (默认模型 models-Real-ESRGAN-anime)
  -g gpu-id            gpu，-1使用CPU，默认0 多GPU可选 0,1,2
  -j load:proc:save    解码/处理/保存的线程数 (默认1:2:2) 多GPU可以设 1:2,2,2:2
  -x                   开启tta模式
  -f format            输出格式(jpg/png/webp, 默认ext/png)
```
