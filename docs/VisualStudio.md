# Visual Studio 开发流程

## 一键生成

在资源管理器中双击仓库根目录的 `GenerateSolution.cmd`，或在 PowerShell 中执行：

```powershell
.\tools\GenerateSolution.ps1
.\tools\GenerateSolution.ps1 -Open
```

脚本可以从任意工作目录调用，通过自身位置找到仓库。它检测安装了 C++ 工具的 Visual Studio，优先使用带 IDE 的版本。只有 Build Tools 时也能生成和编译。CMake 优先从已安装的 VS 工具中选择兼容版本，避免 PATH 中的旧 CMake。Python 3.10+ 需要通过 `python` 命令可用。

缺少的依赖自动按 `dependencies.lock.json` 下载/校验。已有依赖时不需要网络。脚本只规范化子进程环境，处理 MSBuild 遇到的 `PATH`/`Path` 重复问题，不修改系统环境变量。CMD 入口的执行策略参数也仅对该 PowerShell 进程生效。

默认生成 `out/build/vs2022/Hyperion.sln`；选择 VS 2026 时为 `out/build/vs2026/Hyperion.sln`。Debug 与 Release 共用一个 `.sln`，在 IDE 顶部切换。原有 Ninja 构建目录独立保留。

## 在解决方案里工作

1. 打开 `Hyperion.sln`，选择 `Debug | x64` 或 `Release | x64`。
2. 默认启动项目为 `hyperion_viewer`，按 F5 编译并调试渲染器；Ctrl+F5 不附加调试器运行。已有 `.suo` 的启动偏好可能优先，此时手动设一次启动项目。
3. `Hyperion/Applications` 放 Viewer 和独立 AssetTool，`Hyperion/Runtime` 放运行时模块，`Hyperion/Backends` 放图形后端，`Hyperion/Plugins` 放实验插件，`Hyperion/Tests` 放测试，`ThirdParty` 放依赖。模块源码显示在各自的 `Public` / `Private` 筛选器中；Viewer 项目也列出 shader、实验配置和文档。
4. HLSL 在工程里用于查看/编辑，实际由引擎 DXC wrapper 编译，不走 Visual Studio 的默认 FXC 规则。

Viewer 的调试工作目录为仓库根目录，可以在项目属性的“调试 → 命令参数”中填写 `--config experiments/Triangle.json` 等选项。生成的可执行文件在 `out/build/vs2022/bin/Debug` 或 `bin/Release`，所需 DLL 会自动复制到旁边。

Viewer 构建会先运行 native_sample_content，用 C++ AssetTool 将源样例转换到 out/content；Assets 和 Viewer 仅消费生成的 .hasset。新增 native_asset_tests、publication_tests 也纳入 hyperion_check。导入、自定义类型、场景保存和迁移命令见 [NativeAssets.md](NativeAssets.md)。

## 在 VS 中跑测试

展开 `Hyperion/Tests`，**右键 `hyperion_check` → 生成（Build）**。它会先构建测试和 Viewer，再按当前配置运行完整 CTest 套件；失败会让该项目构建失败。输出窗口显示各项测试结果，日志位于构建目录的 `Testing/Temporary/LastTest.log`。

需要调试单个 C++ 测试时，将 `core_tests`、`task_tests`、`config_tests`、`asset_tests`、`shader_tests`、`graph_tests` 、`gui_tests`、`rhi_contract_tests` 或 `d3d12_device_tests` 设为启动项目并按 F5。测试调试工作目录与 CTest 保持一致，避免把临时文件写入源码目录。调试结束后切回 `hyperion_viewer`。

当前测试是由 CTest 驱动的独立可执行程序和 Python 验收脚本，没有接入 VS Test Explorer 适配器，所以完整套件通过 `hyperion_check` 运行。GPU 测试需要可用的 DX12 硬件和桌面会话。右键执行即可，不需要先手动生成整个解决方案。

## 命令行和重新生成

```powershell
# 显式选择 VS，生成后打开 IDE
.\tools\GenerateSolution.ps1 -VisualStudioVersion 2022 -Open

# 生成并编译
.\tools\GenerateSolution.ps1 -Build -Configuration Debug

# 生成、编译所需项目、运行完整测试
.\tools\GenerateSolution.ps1 -Test -Configuration Debug
.\tools\GenerateSolution.ps1 -Test -Configuration Release

# VS/SDK 或编译器选择发生变化时，刷新专用构建目录中的 CMake 缓存
.\tools\GenerateSolution.ps1 -Fresh
```

平时新增目标、修改 CMake 文件后重新运行默认脚本即可。它复用对应构建目录，不做递归删除。CMake 的 `ZERO_CHECK` 也会在构建时检测配置变化。请修改源码中的 `CMakeLists.txt`/`cmake` 文件；直接编辑生成的 `.sln` 或 `.vcxproj` 会在重新生成时被覆盖。`.sln` 和编译产物都属于 `out`，无需提交 Git。

在已经配置好兼容 CMake 的终端中，也可以使用原生命令：

```powershell
cmake --build out/build/vs2022 --config Debug --parallel 8
cmake --build out/build/vs2022 --config Debug --target hyperion_check
ctest --test-dir out/build/vs2022 -C Debug --output-on-failure
```

最后一条只运行已构建的测试；前两条调用 MSBuild。未安装请求的 VS/C++ 组件、没有兼容 CMake、依赖下载失败或编译/测试失败时，脚本会报错并返回非零状态。

## Tracy 与网络监听

`HYP_ENABLE_TRACY` 默认 `OFF`。普通 Viewer 和测试 exe 保留应用时钟、内存统计和 GUI 帧间隔曲线；scope 宏被编译移除，不再执行本地 scope 累计计时，也不启动 Tracy 的 TCP 监听和 UDP 发现广播。以前强制启用的 `TRACY_ENABLE=ON` 缓存会在重新配置时按此选项更新；需要重新编译已有 exe 才生效。

需要连接 Tracy Profiler 时显式开启，结束后恢复关闭：

```powershell
.\tools\GenerateSolution.ps1 -Tracy -Build
.\tools\GenerateSolution.ps1 -NoTracy -Build

# Ninja 构建使用相同开关
.\tools\Build.ps1 -Tracy
.\tools\Build.ps1 -NoTracy
```

原生 CMake 对应 `-DHYP_ENABLE_TRACY=ON/OFF`。开关作用于该构建目录中所有链接 Core 的 exe；同一 VS 解决方案的 Debug/Release 共用此选项，各配置需要分别重编译。不传开关会保留该构建目录的选择；全新配置（或 `-Fresh`）默认关闭，除非同时传入 `-Tracy`。

Tracy 的 `TRACY_ON_DEMAND` 只延迟采集，仍会监听连接，因此启用 Tracy 的 exe 可能触发 Windows 防火墙授权。Windows 的此类提示主要针对入站监听，是否再次提示取决于 exe 路径、网络配置文件、已有防火墙规则和系统策略，并非所有出站联网都弹窗，详见 [微软防火墙规则说明](https://learn.microsoft.com/en-us/windows/security/operating-system-security/network-security/windows-firewall/rules)。此构建选项只控制 Tracy；显式加载 RenderDoc 或以后引入其他网络功能时，应由对应功能按需启用。构建脚本不修改防火墙规则或通知设置。

默认关闭 Tracy 时，CTest 的 `offline_startup` 会启动真实 Viewer，在启动及开始渲染后检查该进程的 TCP/UDP 端点，防止默认监听再次引入。

性能测量推荐独立的 `profile` preset：`.\tools\Build.ps1 -Preset profile` 使用 Release 优化并给自有模块生成 `/Zi`、可执行文件链接 `/DEBUG:FULL /INCREMENTAL:NO`。编入支持后仍需 `--profile` 或 GUI 开关启用 scope；细节、GPU 和系统采样分别选择。新配置的 Tracy 默认仅监听 loopback，并关闭发现广播；已有显式缓存选择保持不变。完整采集、导出命令与开销证据见 [性能分析](Profiling.md)。

可选 RenderDoc 支持通过 `-RenderDoc` 编入、`-NoRenderDoc` 关闭；不指定时保留 CMake 缓存。运行时仍需 `--renderdoc` 加载插件，详见 [RenderDoc 抓帧](RenderDoc.md)。测试入口与结果记录约定见 [验证指南](Verification.md)。
