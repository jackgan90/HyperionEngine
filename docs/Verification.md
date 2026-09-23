# 构建与验证指南

测试定义以 [Source/Tests/CMakeLists.txt](../Source/Tests/CMakeLists.txt) 为准。测试数量受构建选项影响，不以历史验收记录中的数量作为当前基线。

## 常规检查

从仓库根目录执行：

```powershell
python tools/CheckStyle.py --paths-only
python tools/CheckBoundaries.py
git diff --check

# 构建并运行完整 CTest
./tools/Build.ps1 -Preset debug -Test
./tools/Build.ps1 -Preset release -Test

# Visual Studio 工作流
./tools/GenerateSolution.ps1 -Test -Configuration Debug
```

C++/HLSL 变更的完整格式和语义命名检查见 [CodingStyle.md](CodingStyle.md)。纯文档修改检查描述与源码一致性、相对链接和差异即可。

## 查询与选择测试

```powershell
# 已配置并构建 Ninja Debug 后查询当前测试
ctest --preset debug -N

# 运行相关子集，CTest 自动加入必需的 fixture
ctest --preset debug --output-on-failure -R "scene_management|scene_viewer_controls"

# 自动化基础与真实 CLI/MCP 进程；只使用隔离夹具
ctest --preset debug --output-on-failure -R '^automation_'

# 内容根目录：共享切换契约、Editor 空启动/偏好恢复、原生资产工具
ctest --preset debug --output-on-failure -R 'automation_|editor_content|native_publication_cli'
```

内容根回归应覆盖未设置 `/Game` 时的可用状态、运行时设置/清空、dirty/busy/stale 拒绝、只读权限、旧文档失效和参与者停机撤销。准备或校验失败必须保留当前内容；Editor 与 CLI/MCP 使用同一个领域服务。测试夹具通过显式目录或内存挂载提供资源，不生成挂载配置文件。

CTest 不代替构建。Visual Studio 使用 `ctest --test-dir out/build/vs2022 -C Debug`；实际目录随生成器选择变化。GPU、窗口和截图验收需要可用的 D3D12 设备与桌面会话。RenderDoc 回放需要编入支持并安装兼容运行库；Tracy trace 验收需要对应构建与工具，见 [RenderDoc.md](RenderDoc.md)、[Profiling.md](Profiling.md)。

## 有限帧截图

```powershell
./out/build/debug/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --frames 120 --capture out/captures/triangle.png --verify-triangle --verify-ui
./out/build/debug/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --config experiments/Model.json --frames 180 --hidden --no-ui --capture out/captures/model.png --verify-model
```

像素验收必须同时提供有限 `--frames` 和 `--capture`。`--hidden` 使用隐藏窗口，仍执行真实 GPU 工作；模型验证检查资源和绘制就绪。较慢设备或较大资产需要足够加载帧数。`--exercise-window` 验证缩放、最小化和恢复；`--verify-clear` 使用空插件集合检查清屏。

## 结果记录

CTest 日志保存在构建目录的 `Testing/Temporary/LastTest.log`，生成夹具、截图和测量日志位于 `out`，不提交。记录验证时应写明源码版本、配置、启用选项、实际测试命令和结果；GPU 性能数据同时记录硬件、场景就绪、视图/draw 数、预热、VSync 和验证层状态。

[文档索引](README.md) 列出历史审计和性能数据。它们说明对应日期与版本的结果，不证明当前工作区已重新执行同一套测试；原始 OpenSpec 归档保留历史设计与验收证据。

## 应用附着回归

构建 `hyperion_editor`、`hyperion_viewer`、`hyperion_automation_cli`、`automation_scene_tests`、`automation_connection_tests` 和 `transport_tests` 后，运行 `ctest --test-dir out/build/debug -R "^(transport_contracts|automation_scene|automation_connections|automation_attachment)$" --output-on-failure`。真实应用验收使用显式 HyperionAssets 目录和隔离输出文件，不修改输入资产或用户已运行的应用。

涉及 SceneEditing 的变更还须运行 `editor_state`、`editor_acceptance`、`editor_multiselect`、`editor_placement`、`editor_content_transition`、`scene_viewer_controls` 和 `plugin_applications`，保护 GUI 历史、手势、选择、保存、换根及 provider 缺失路径。通信故障应验证无目标回退、无变更重放、断连后的任务排空和未知 scheme。
