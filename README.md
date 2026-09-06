# Hyperion

C++20 渲染实验框架。当前实现 Windows x64 / D3D12：通过静态插件和 Render Graph 绘制三角形，配有可交互的调试面板。

## 构建和运行

需要 Visual Studio C++ x64 Build Tools、Windows SDK、Visual Studio 的 CMake/Ninja 工具组件，以及 Python 3.10+。CMake 要求 3.28+；脚本优先使用 Visual Studio 自带的版本。本机已验证 Ninja / MSVC 19.50 和 VS 2022 / MSVC 19.38 两条构建路径，GPU 为 RTX 5080。

### Visual Studio / `.sln`（日常开发入口）

双击根目录的 **`GenerateSolution.cmd`** 即可生成解决方案；也可以执行：

```powershell
.\tools\GenerateSolution.ps1 -Open
```

脚本自动检测 VS/CMake 并准备缺少的锁定依赖。本机默认使用 VS 2022，生成 `out/build/vs2022/Hyperion.sln`。打开后选择 `Debug | x64` 或 `Release | x64`，默认启动项目为 `hyperion_viewer`，按 F5 调试。

在解决方案的 `Hyperion/Tests` 文件夹中，**右键 `hyperion_check` → 生成**，即可先编译所需项目，再运行完整测试套件。公共头文件、shader 和实验配置已加入工程筛选器，方便浏览。

```powershell
# 一条命令生成、编译并跑测试
.\tools\GenerateSolution.ps1 -Test -Configuration Debug
.\tools\GenerateSolution.ps1 -Test -Configuration Release
```

更多参数、单个测试调试、Test Explorer 说明和重新生成方法见 [Visual Studio 开发流程](docs/VisualStudio.md)。

### Ninja 构建

```powershell
python tools/Bootstrap.py
.\tools\Build.ps1 -Preset debug -Test
.\tools\Build.ps1 -Preset release -Test
.\out\build\release\bin\hyperion_viewer.exe
```

首次 bootstrap 从官方上游下载依赖。15 个依赖的版本、commit 和 SHA-256 固定在 `dependencies.lock.json`；源码位于 `out/deps`，下载缓存位于 `out/downloads`。后续构建无需网络，构建时检查依赖标记与 lock 一致。脚本只为当前进程配置编译环境，不修改系统 PATH。

也可以在已配置好的 x64 Developer PowerShell 中使用 `cmake --preset debug`、`cmake --build --preset debug` 和 `ctest --preset debug --output-on-failure`。设置 `BUILD_TESTING=OFF` 可仅构建框架和 Viewer。

## 实验操作

默认配置为 `experiments/Triangle.json`。面板可实时修改三角形缩放、背景色和 VSync；“Save experiment”保存到当前配置文件，“Capture screenshot”写入 `out/captures`。插件列表修改后重启生效。窗口右上角关闭按钮退出应用。

```powershell
# 使用独立实验配置
.\out\build\release\bin\hyperion_viewer.exe --config experiments/Triangle.json

# 有限帧运行、截图及像素验收
.\out\build\release\bin\hyperion_viewer.exe --frames 120 --capture out/captures/final.png --verify-triangle --verify-ui

# 三角形，不显示调试 UI
.\out\build\release\bin\hyperion_viewer.exe --no-ui
```

辅助参数：`--hidden` 创建隐藏窗口用于 GPU 测试；`--exercise-window` 自动缩放、最小化和恢复；`--save-config <path>` 在退出前保存配置与窗口尺寸；`--verify-clear` 使用空插件集合并检查清屏像素。截图验收需要同时提供 `--frames` 和 `--capture`。日志位于 `out/logs/viewer.log`。

## 实现边界

- Main 管理窗口、输入和 GUI；Render 组织帧；多个专用 RHI 线程录制命令，RHI 0 提交。oneTBB 执行通用 CPU jobs，支持依赖、异常传播和等待。
- 逻辑插件通过 ID、依赖和启动/关闭生命周期组织，当前为静态链接。已有 `triangle`、`debug-ui`，不涉及 DLL 热更新。
- 反射使用手写类型/属性描述符，驱动配置、资产引用保存和 GUI 编辑。没有 GC、AST 生成器或对象图序列化。
- 首版 Render Graph 管理一个导入的交换链颜色目标，校验 pass 依赖、内容初始化及状态转换。容量由后端能力限定；当前 D3D12 支持最多 15 个颜色 pass（另占一个 Present context）、单 graphics queue、2 个 GPU frame context、256 个采样纹理描述符。
- 数学使用引擎自有列主序类型；资产层支持单个 glTF 三角形 primitive、PNG 与 EXR。没有完整场景、材质或动画导入。
- RHI 由 `IRHIBackend`、`IRHIDevice`、`IRHISwapchain` 抽象接口与独立后端组成，设备可以脱离窗口创建。通过配置 `rhi_backend` 或 `--backend d3d12` 选择；尚未注册的 Vulkan/Metal 会明确报错。
- HLSL 可生成 DXIL、SPIR-V 和 MSL 文本。实际运行的是 DX12；Vulkan/Metal 运行时与移动平台尚未实现。
- CPU 内存统计覆盖内部 allocator/PMR，以及接入回调的 cgltf、GUI 和 D3D12MA 元数据；不代表进程总内存。GPU 统计覆盖 D3D12MA 资源，不包含交换链和驱动内部占用。
- Tracy 已启用按需连接，支持 CPU scope 和显式内存事件。GUI 曲线显示包含 Present 等待的 CPU 帧间隔，不是 GPU timestamp。

## 目录和扩展

默认代码风格参考 Unreal Engine：`Hyperion` 命名空间，PascalCase 标识符和文件名，`F`/`E`/`I` 类型前缀，Allman 大括号、4 列 Tab 和 `.h` 头文件。布尔变量也遵循大写开头要求。规范、例外和检查命令见 [代码规范](docs/CodingStyle.md)；后续开发同时遵循根目录 `AGENTS.md`。

`Source/Runtime` 按 Core、Tasks、Math、Reflection、Assets、RHI、Renderer 等概念组织，每个模块有独立的 `Public` / `Private` 和 CMake target。`Source/Backends` 放原生图形后端；`Source/Plugins` 放实验与 UI 插件；`Source/Applications` 放应用；`Source/Tests` 放测试。`shaders` 放 HLSL；`openspec` 保存规范与归档。后续 Scene、Animation 将作为独立 Runtime 模块接入，详见 [源码组织与扩展位置](docs/SourceLayout.md)。

依赖必须通过引擎 wrapper 使用，公共头文件不暴露第三方类型。执行 `python tools/CheckBoundaries.py` 检查 include 边界。参考 [架构和扩展约定](docs/Architecture.md)、[依赖列表](docs/Dependencies.md) 和 [九步实施路线](docs/Roadmap.md)。

## 版本控制

Git 保存源码、测试、Shader、实验配置模板、CMake 和工具脚本、`dependencies.lock.json`、代码规范配置、文档、OpenSpec 规范及归档，以及 `.codex/skills` 中的项目工作流。这些文件组成可重新生成工程的项目基线。

依赖源码与下载缓存、编译产物、运行日志、截图和临时脚本统一放在 `out/`，不提交。`.sln`、`.vcxproj` 等工程文件通过 `GenerateSolution.cmd` 重新生成；IDE 用户状态、`CMakeUserPresets.json`、本地环境覆盖文件和 Python 缓存也由 `.gitignore` 排除。共享构建选项写入 `CMakePresets.json`，本机路径写入被忽略的用户预设。

提交前用 `git status --short --untracked-files=all` 检查遗漏，再用 `git diff --cached --stat` 和 `git diff --cached --check` 检查暂存内容。不要通过忽略整个源码目录来隐藏未完成的修改。

## 许可证

本项目采用 [MIT License](LICENSE)。第三方依赖仍遵循各自的许可证。
