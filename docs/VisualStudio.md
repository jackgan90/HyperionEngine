# Visual Studio 开发流程

## 一键生成

在资源管理器中双击仓库根目录的 `GenerateSolution.cmd`，或在 PowerShell 中执行：

```powershell
.\tools\GenerateSolution.ps1
.\tools\GenerateSolution.ps1 -Open
```

脚本可以从任意工作目录调用，通过自身位置找到仓库。它检测安装了 C++ 工具的 Visual Studio，优先使用带 IDE 的版本；当前本机会选择 VS 2022 Community。只有 Build Tools 时也能生成和编译。CMake 优先从已安装的 VS 工具中选择兼容版本，避免 PATH 中的旧 CMake。Python 3.10+ 需要通过 `python` 命令可用。

缺少的依赖自动按 `dependencies.lock.json` 下载/校验。已有依赖时不需要网络。脚本只规范化子进程环境，处理 MSBuild 遇到的 `PATH`/`Path` 重复问题，不修改系统环境变量。CMD 入口的执行策略参数也仅对该 PowerShell 进程生效。

默认生成 `out/build/vs2022/Hyperion.sln`；选择 VS 2026 时为 `out/build/vs2026/Hyperion.sln`。Debug 与 Release 共用一个 `.sln`，在 IDE 顶部切换。原有 Ninja 构建目录独立保留。

## 在解决方案里工作

1. 打开 `Hyperion.sln`，选择 `Debug | x64` 或 `Release | x64`。
2. 默认启动项目为 `hyperion_viewer`，按 F5 编译并调试渲染器；Ctrl+F5 不附加调试器运行。已有 `.suo` 的启动偏好可能优先，此时手动设一次启动项目。
3. `Hyperion/Applications` 放 Viewer，`Hyperion/Runtime` 放运行时模块，`Hyperion/Backends` 放图形后端，`Hyperion/Plugins` 放实验插件，`Hyperion/Tests` 放测试，`ThirdParty` 放依赖。模块源码显示在各自的 `Public` / `Private` 筛选器中；Viewer 项目也列出 shader、实验配置和文档。
4. HLSL 在工程里用于查看/编辑，实际由引擎 DXC wrapper 编译，不走 Visual Studio 的默认 FXC 规则。

Viewer 的调试工作目录为仓库根目录，可以在项目属性的“调试 → 命令参数”中填写 `--config experiments/Triangle.json` 等选项。生成的可执行文件在 `out/build/vs2022/bin/Debug` 或 `bin/Release`，所需 DLL 会自动复制到旁边。

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

## 本机验证记录

可选 RenderDoc 抓帧支持可通过 `-RenderDoc` 编入，通过 `-NoRenderDoc` 关闭；不指定时保留 CMake 缓存。完整说明见 [RenderDoc 抓帧](RenderDoc.md)。2026-09-06 验证：启用时 Debug/Release 分别通过 **28/28**；关闭时 Debug 通过 **24/24**，并验证了未编译插件的明确错误。日志为 `out/RenderDocDebugTest.log`、`out/RenderDocReleaseTest.log`、`out/RenderDocDisabledTest.log`；验收后已恢复启用支持的 Debug 构建。

同日 RenderDoc 独立审计后，修复内容验收误报与清理失败后的所有权丢失，并增加 `capture_failure_recovery` 回归。最终启用时 Debug/Release 分别通过 **29/29**，关闭时 Debug 通过 **24/24**；格式、模块边界、51 个编译单元的命名检查及 OpenSpec strict 全部通过。日志为 `out/RenderDocReviewDebug.log`、`out/RenderDocReviewRelease.log`、`out/RenderDocReviewDisabled.log`，复核过程见 [RenderDoc 独立审计](RenderDocReview.md)。当前 VS 工程已恢复启用支持并重新构建 Debug。

2026-09-06：VS 2022 Community / MSBuild 17 / MSVC 19.38，Debug 与 Release 均通过 `hyperion_check` 完成 **13/13** CTest 验收，包含 DX12 硬件测试。CMD 入口也已从仓库外的工作目录执行，并成功重复生成。日志为 `out/vs-workflow-debug.log`、`out/vs-workflow-release.log` 和 `out/vs-workflow-cmd.log`。

同日完成 UE 风格迁移后，再次生成并验证 Debug/Release，均为 **14/14**（新增 `code_style_paths`），记录为 `out/style-vs-debug.log`、`out/style-vs-release.log`。CMD 入口也已复验，记录为 `out/style-cmd.log`。代码文件和辅助脚本已采用 PascalCase，解决方案名称、启动项目和 `hyperion_check` 使用方式保持一致；Viewer 项目中可以直接浏览规范和格式配置。

同日完成 RHI 抽象与源码模块化后，Debug/Release 均为 **16/16**，新增 `rhi_backend_contracts` 与 `d3d12_device_ownership`。记录为 `out/ModularDebugTest.log`、`out/ModularReleaseTest.log`。解决方案仍由同一入口生成，项目按 `Hyperion/Runtime`、`Backends`、`Plugins`、`Applications`、`Tests` 分组，模块内的 Public/Private 与源码目录一致。

同日完成异步 glTF 资产流水线后，Debug/Release 均为 **23/23**，记录为 `out/GltfVsDebugVerified.log`（41.62 秒）与 `out/GltfVsReleaseVerified.log`（37.39 秒）。新增 IO、Archive、Scene、importer、材质像素与模型 Viewer 验收；涵盖单 Worker、延迟读取、取消、保存失败收尾和真实 D3D12 截图。并行构建曾暴露每个程序重复复制同一 DLL 的竞态，现由共享 `hyperion_runtime_files` 目标统一准备 DLL。两种配置均已通过此修复后的完整流程。示例运行见 [AssetPipeline.md](AssetPipeline.md)。

同日 Model Viewer 独立审计修复后，Debug/Release 再次通过 **23/23**，记录为 `out/ModelReviewDebug.log`（35.62 秒）与 `out/ModelReviewRelease.log`（28.13 秒）。原测试中新增 sparse/interleaved 组合、畸形范围、倒序深层级、偏轴透明像素与常量偏移回归；审计及修复范围见 [ModelViewerReview.md](ModelViewerReview.md)。
