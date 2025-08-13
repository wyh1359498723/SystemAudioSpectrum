# SystemAudioSpectrum

基于 Qt + WASAPI + kissfft 的系统音频频谱可视化工具。实时从 Windows 默认输出设备进行环回采集，执行 0–20kHz 频率范围的 FFT 分析，并以 20 段彩色柱状图在独立渲染线程中动态绘制，主线程仅用于窗口显示（不参与音频处理与绘制）。

仓库地址：[wyh1359498723/SystemAudioSpectrum](https://github.com/wyh1359498723/SystemAudioSpectrum.git)

## 功能特性

- 实时捕获系统默认输出设备（环回），无需加载本地音频文件
- 完整频率范围 0–20kHz 的频谱分析
- 20 段柱状图（对数频段划分，含 DC），约每 30ms 刷新
- 彩色渐变 + 峰值指示 + 衰减效果
- 子线程完成音频捕获、FFT 和渲染，主线程仅负责 UI 显示

## 系统架构

- 线程模型
  - AudioCapture（QThread）：WASAPI 环回采集，单声道 float 样本写入环形缓冲
  - FFTProcessor（QThread）：从环形缓冲取样、Hann 窗、kissfft（实数 FFT）计算、聚合 20 频段并归一化
  - Renderer（QThread）：根据 20 段能量生成 QImage 帧（彩色柱状+峰值），30ms 一帧
  - UI（QWidget）：定时从渲染线程获取最新帧并绘制到窗口，不参与计算
- 数据流
  - WASAPI Loopback → RingBuffer → FFTProcessor → Renderer(QImage) → UI 绘制

## 实现细节

- 音频捕获（WASAPI 环回）
  - 获取默认渲染设备（`IMMDeviceEnumerator::GetDefaultAudioEndpoint(eRender, eConsole)`）
  - `IAudioClient` 共享模式 + `AUDCLNT_STREAMFLAGS_LOOPBACK`
  - 通过 `IAudioCaptureClient` 读取帧；支持 32-bit float 与 16-bit PCM 转单声道 float
  - 写入线程安全 `RingBuffer`
- RingBuffer
  - 简单加锁环形缓冲，支持 `write`/`read`/`availableToRead` 等
- FFT 分析（kissfft）
  - FFT 大小：2048，Hop 大小：512
  - Hann 窗；`kiss_fftr` 进行实数 FFT；幅度 `sqrt(re^2+im^2)`
  - 20 段对数频段（20Hz–20kHz，第一段包含 DC）；每段取区间内幅度平均
  - 归一化与 gamma 矫正（默认 γ=0.5）使视觉更平滑
- 渲染
  - 独立线程按 ~30ms 刷新，离屏生成 `QImage(Format_RGBA8888)`
  - 每段柱状条采用 HSV 按序彩色渐变，亮度随高度变化
  - 峰值指示条带衰减（默认每帧 -0.02）

## 目录结构（关键文件）

- `SystemAudioSpectrum.h/.cpp`：主窗口，定时取帧并显示
- `src/AudioCapture.h/.cpp`：WASAPI 环回采集线程
- `src/RingBuffer.h/.cpp`：线程安全环形缓冲
- `src/FFTProcessor.h/.cpp`：FFT 线程 + 20 段聚合
- `src/Renderer.h/.cpp`：渲染线程（QImage 离屏绘制）
- `thirdparty/kissfft`：FFT 库

## 构建与运行

先决条件：
- Visual Studio 2022（Desktop development with C++）
- Windows 10/11 SDK
- CMake 3.16+（VS 自带或本地安装）
- Qt 6.9.0 MSVC 2022 64-bit（你提供的路径：`D:\Qt\6.9.0\msvc2022_64`）
- 可选：Ninja

推荐（VS 开发者命令行）：
```bat
:: 打开 “x64 Native Tools Command Prompt for VS 2022”
cd /d E:\qt_project\SystemAudioSpectrum
rmdir /s /q out\build\debug

cmake -S . -B out/build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/Qt/6.9.0/msvc2022_64
cmake --build out/build/debug -j

:: 运行（如缺 Qt 运行库，先执行 windeployqt）
D:\Qt\6.9.0\msvc2022_64\bin\windeployqt.exe --debug .\out\build\debug\SystemAudioSpectrum.exe
.\out\build\debug\SystemAudioSpectrum.exe
```

VS 生成器替代方案：
```bat
cmake -S . -B out/build/msvc -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=D:/Qt/6.9.0/msvc2022_64
cmake --build out/build/msvc --config Debug
.\out\build\msvc\Debug\SystemAudioSpectrum.exe
```

VS IDE 内使用（CMake 项目）：
- CMake → Delete Cache and Reconfigure
- CMake Variables 建议添加：
  - `CMAKE_PREFIX_PATH=D:/Qt/6.9.0/msvc2022_64`
  - `KISSFFT_PKGCONFIG=OFF`
  - `KISSFFT_TEST=OFF`
  - `KISSFFT_TOOLS=OFF`
- 选择 MSVC x64 工具集与有效的 Windows SDK

## 运行效果

- 打开窗口时自动从系统默认输出设备环回采集
- 20 段彩色柱状图动态变化，峰值条随时间缓慢衰减
- 刷新周期 ~30ms（≈33 FPS）

## 常见问题与排查

- 找不到 `kernel32.lib`
  - 未在 VS 开发者命令行或未安装 Windows SDK；用 “x64 Native Tools Command Prompt for VS 2022” 重新配置
- `Cannot specify link libraries for target ... not built by this project`
  - 通常是配置阶段失败导致目标未创建；先修复上游错误，Delete Cache 后重新配置
- 找不到 Qt
  - 指定 `-DCMAKE_PREFIX_PATH=D:/Qt/6.9.0/msvc2022_64`
- pkg-config 相关报错
  - CMake 中已关闭 kissfft 的 pkg-config/测试/工具：`KISSFFT_PKGCONFIG=OFF`、`KISSFFT_TEST=OFF`、`KISSFFT_TOOLS=OFF`

## 可配置项（在源码中调整）

- FFT 窗长度与步长：`src/FFTProcessor.h/.cpp` 中 `m_fftSize`（默认 2048）、`m_hopSize`（默认 512）
- 窗函数：默认 Hann，可替换实现
- 频段数量与频率分布：`computeBands` 中（默认 20 段，20Hz–20kHz 对数）
- 归一化与 γ 矫正：`computeBands` 末尾（默认 γ=0.5）
- 刷新间隔：`Renderer::run` 中（默认 30ms）
- 柱状配色与峰值衰减：`Renderer.cpp` 中 `colorForValue` 与 `decay=0.02`

## 许可

- 本项目集成的 kissfft 遵循其仓库自带的许可证（BSD-3-Clause/Unlicense，见 `thirdparty/kissfft/LICENSES`）。
- 本项目其他部分如无特别声明，默认以仓库根目录许可为准。

---
本文档对应仓库：[wyh1359498723/SystemAudioSpectrum](https://github.com/wyh1359498723/SystemAudioSpectrum.git)
