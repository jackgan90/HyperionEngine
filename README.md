# Hyperion

Hyperion 是一个 C++20 渲染实验框架，当前运行平台为 Windows x64 / D3D12。项目提供模块化运行时、原生资产工具、交互式 Viewer 和独立场景编辑器，用于开发与验证渲染功能。

## 功能

- 共享线性 HDR Forward / Deferred 管线、可配置 GBuffer、Reinhard 色调映射和启动时选择的标准 Z / reversed-Z。
- 四级方向光阴影、点光与聚光、默认启用的 CPU 聚簇光照，以及天空背景和 SH/GGX 图像光照。
- 可配置参数与资源的 compute pipeline、按消费者请求生成的 HZB，以及可动态切换的 Deferred contact shadows。
- 静态 glTF/GLB 离线导入，独立模型、材质、纹理、场景和天空 `.hasset` 资产，异步加载与共享资源管理。
- 场景层级、相机和光源节点编辑、原生场景保存、BVH 视锥剔除、实例批处理和增量渲染准备。
- 深色可停靠场景编辑器，以及独立的多页签资产编辑器窗口；Content Browser 双击打开 Texture、Model、Sky、Material，可同时观察场景和资产，支持预览、简单属性编辑、保存、每文档 Undo/Redo 和保存后引用刷新。详见 [编辑器](docs/Editor.md)。
- Main / Render / RHI CPU 帧管线、独立 IO 线程和 oneTBB Worker 任务；反射配置、调试 GUI，以及可选 Tracy 和 RenderDoc 集成。
- Viewer / Editor 共享静态插件宿主，支持启动依赖、统一生命周期、类型化服务、作用域事件和渲染阶段扩展；详见 [插件系统](docs/PluginSystem.md)。
- Agent 自动化宿主提供 CLI、JSONL 和 MCP stdio，共享操作目录、按需 schema 查询及异步任务；独立资产操作与 Editor 共用领域逻辑，可通过本机连接附着 Editor/Scene Viewer，直接查询、变换和保存当前场景，Editor 与 agent 共享撤销历史。用法与覆盖范围见 [自动化接口](docs/Automation.md)。

Vulkan/Metal 尚无运行时后端；SPIR-V/MSL 支持编译与反射。静态模型导入不支持动画、蒙皮、morph target 或压缩 glTF 扩展。具体边界见 [资产管线](docs/AssetPipeline.md) 和 [功能范围](docs/Roadmap.md)。

## 构建

需要 Visual Studio C++ x64 工具、Windows SDK、CMake 3.28+ 和 Python 3.10+；Ninja 构建路径还需要 Ninja。构建脚本可检测 Visual Studio 自带的工具。

双击根目录 `GenerateSolution.cmd` 生成 Visual Studio 解决方案，或运行：

```powershell
./tools/GenerateSolution.ps1 -Open
```

在生成的解决方案中选择 `Debug | x64`、`Release | x64` 或 `RelWithDebInfo | x64`，启动项目为 `hyperion_viewer`。RelWithDebInfo 使用 O2 优化并生成 PDB 调试信息。生成 `hyperion_check` 项目会构建并运行测试。更多选项见 [Visual Studio 开发流程](docs/VisualStudio.md)。

使用 Ninja 构建并测试：

完整测试还需检出同级 HyperionAssets、执行 `git lfs pull`，并用 `python tools/PrepareContent.py --restore-only` 准备源导入测试缓存。

```powershell
python tools/Bootstrap.py
./tools/Build.ps1 -Preset debug -Test
./tools/Build.ps1 -Preset release -Test
./tools/Build.ps1 -Preset relwithdebinfo -Test
```

依赖版本、commit 和 SHA-256 固定在 `dependencies.lock.json`。首次准备依赖需要下载；依赖源码和缓存位于 `out/deps`、`out/downloads`。Viewer 构建直接使用已发布资源；示例需检出同级 HyperionAssets 并执行 `git lfs pull`。目录选择、离线重建及源导入测试准备见 [Content 与虚拟文件系统](docs/ContentFileSystem.md)。构建产物、日志与截图保存在被 Git 忽略的 `out` 中。

## 运行

从仓库根目录运行：

```powershell
# 场景编辑器：启动后选择 File > Open Scene... > /Game/Scenes/Sponza.hasset
./out/build/release/bin/hyperion_editor.exe

# Sponza 场景
./out/build/release/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --config experiments/Scene.json

# 静态模型
./out/build/release/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --config experiments/Model.json

# 三角形实验，也是无参数启动时的默认配置
./out/build/release/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --config experiments/Triangle.json
```

SceneViewer 的 WASDQE 用于连续移动，右键拖动环绕，滚轮推拉，Home 取景，Tab 切换面板。Sponza 默认显示调试面板，可编辑相机、模型、光源和天空并保存场景。详见 [场景管理](docs/SceneManagement.md) 和 [Sponza 示例](docs/SponzaMigration.md)。

Editor 使用独立视口相机：按住右键后 WASDQE 平移，右键拖动调整朝向，滚轮推拉、Home 取景。Outliner 选择对象，Details 通过组件反射编辑属性，支持撤销重做、保存和另存。面板可拖动停靠，布局在退出时保存。启动、挂载和功能范围见 [编辑器](docs/Editor.md)。

自定义模型先离线导入，再交给 Viewer：

```powershell
./out/build/release/bin/hyperion_asset_tool.exe --asset-root ../HyperionAssets import path/to/model.glb /Game/Models/Custom.hasset --library /Game
./out/build/release/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --model /Game/Models/Custom.hasset
```

`--scene` 选择原生场景；`--pipeline forward` 切换到 HDR Forward。有限帧截图及自动验收用法见 [验证指南](docs/Verification.md)。

## 文档与源码

[文档索引](docs/README.md) 汇总使用说明、架构、渲染契约和历史测量记录。

- `Source/Runtime`：按概念划分的运行时模块，各自拥有 Public / Private 和 CMake target。
- `Source/Backends`：原生图形后端；`Source/Plugins`：实验与 UI 插件。
- `Source/Applications`：Editor、Viewer、AssetTool 与 Automation；`Source/Tests`：单元测试和集成验收。
- `Content`：引擎内置资源与 Shader；HyperionAssets：独立样例资源仓库。
- `experiments`：实验配置；`tools`：构建、资产重建和检查工具。
- `openspec`：功能规范与变更归档。

开发遵循 [AGENTS.md](AGENTS.md)、[代码规范](docs/CodingStyle.md) 和 [源码组织](docs/SourceLayout.md)。

## 许可证

项目采用 [MIT License](LICENSE)。第三方依赖和示例资产遵循各自许可证，Sponza 与天空资产的来源见对应资产文档。
