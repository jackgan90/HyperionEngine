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

## 规范审查与工具覆盖

现有自动检查各有范围：

- `CheckStyle.py --paths-only` 检查自有文件命名和 include 路径等路径约定；CTest 的 `code_style_paths` 使用这一模式。
- `CheckStyle.py` 在路径检查之外执行 C++/HLSL 格式检查；`--naming` 另外使用 clang-tidy/clang-query 检查 C++ 语义命名和单变量声明等已配置规则，需要编译数据库和 LLVM。`--format` 会修改文件，不能当作只读检查。
- `CheckBoundaries.py` 检查已编码的模块依赖、include、CMake 声明和第三方隔离规则；它不能证明全部运行时所有权或生命周期契约。

当前工具不检查函数 100 行或非测试 C++ 文件 500 行，也不自动判定显示文本参与逻辑、重复权威定义、隐含位置关系、策略耦合或缓存依赖是否完整。代码审查应按 [代码规模与拆分](CodingStyle.md#代码规模与拆分) 核实行数、例外与拆分边界，并逐项检查 [可维护性与语义表达](CodingStyle.md#可维护性与语义表达) 中的五条规则。通过格式、命名和边界检查只能证明这些工具覆盖的项目通过，不能替代语义审查及相关行为验证。

## 查询与选择测试

```powershell
# 已配置并构建 Ninja Debug 后查询当前测试
ctest --preset debug -N

# 运行相关子集，CTest 自动加入必需的 fixture
ctest --preset debug --output-on-failure -R "scene_management|scene_navigation"

# 自动化基础与真实 CLI/MCP 进程；只使用隔离夹具
ctest --preset debug --output-on-failure -R '^automation_'

# 内容根目录：共享切换契约、Editor 空启动/偏好恢复、原生资产工具
ctest --preset debug --output-on-failure -R 'automation_|editor_content|native_publication_cli'
```

内容根回归应覆盖未设置 `/Game` 时的可用状态、运行时设置/清空、dirty/busy/stale 拒绝、只读权限、旧文档失效和参与者停机撤销。准备或校验失败必须保留当前内容；Editor 与 CLI/MCP 使用同一个领域服务。测试夹具通过显式目录或内存挂载提供资源，不生成挂载配置文件。

CTest 不代替构建。Visual Studio 使用 `ctest --test-dir out/build/vs2022 -C Debug`；实际目录随生成器选择变化。GPU、窗口和截图验收需要可用的 D3D12 设备与桌面会话。RenderDoc 回放需要编入支持并安装兼容运行库；Tracy trace 验收需要对应构建与工具，见 [RenderDoc.md](RenderDoc.md)、[Profiling.md](Profiling.md)。

启动应用/自动化前端的验收 CTest 使用 `hyp_discovery_test`，由 `DiscoveryEnvironment.py` 在 `out/discovery-tests/run-*` 下创建每轮独立的 `LOCALAPPDATA`，Editor、CLI 和 MCP 子进程共享该环境。强制结束留下的记录只保留在该轮目录，不发布到交互用户的发现目录，也不清理用户的历史记录。测试可显式覆盖子进程环境以验证注册失败；脚本不修改用户或系统环境变量。

直接调用共享 `Application` / `Session` 的 Python 验收也会按进程创建或继承该环境。独立运行其他 Editor 验收脚本或 C++ `automation_local_tests` 时，使用同一包装入口：

```powershell
python Source/Tests/Automation/DiscoveryEnvironment.py out/build/debug/bin/automation_local_tests.exe
```

`automation_discovery` 使用真实 Editor、CLI 和 MCP 验证两个枚举上限下的精确附着、原生进程失效检查、超时/连接失败不误删、正常撤销和强制结束后的回收；`automation_connections` 覆盖枚举计数、旧格式、未知所有者、PID 复用、非法记录与删除时的并发修改保护。

## 有限帧截图

```powershell
out/build/debug/bin/hyperion_editor.exe --asset-root ../HyperionAssets --scene /Game/Scenes/Showcase.hasset --frames 180 --hidden --capture out/Showcase.png --layout out/AcceptanceLayout.ini --ui-preferences out/AcceptanceScale.ini --editor-preferences out/AcceptancePreferences.ini
```

有限帧截图同时提供 `--frames` 和 `--capture`。`--hidden` 使用隐藏窗口，仍执行真实 GPU 工作；较慢设备或较大资产需要足够加载帧数。需要精确控制就绪时机时，通过 Automation 查询 `scene.status` 后调用 `render.screenshot`。`editor_render_controls` 覆盖 HUD 缩放与窗口尺寸变化；`plugin_applications` 覆盖最小宿主、插件禁用和缺失输出的受控失败。

## 结果记录

CTest 日志保存在构建目录的 `Testing/Temporary/LastTest.log`，生成夹具、截图和测量日志位于 `out`，不提交。记录验证时应写明源码版本、配置、启用选项、实际测试命令和结果；GPU 性能数据同时记录硬件、场景就绪、视图/draw 数、预热、VSync 和验证层状态。

[文档索引](README.md) 列出当前功能和验证入口；原始 OpenSpec 归档保留历史设计与验收证据，不证明当前工作区已重新执行同一套测试。

## 应用附着回归

构建 `hyperion_editor`、`hyperion_automation_cli`、`automation_scene_tests`、`automation_connection_tests` 和 `transport_tests` 后，运行 `ctest --test-dir out/build/debug -R "^(transport_contracts|automation_scene|automation_connections|automation_attachment)$" --output-on-failure`。真实应用验收使用显式 HyperionAssets 目录和隔离输出文件，不修改输入资产或用户已运行的应用。

涉及 SceneEditing 的变更还须运行 `editor_state`、`editor_acceptance`、`editor_multiselect`、`editor_placement`、`editor_content_transition`、`scene_navigation` 和 `plugin_applications`，保护 GUI 历史、手势、选择、保存、换根及 provider 缺失路径。通信故障应验证无目标回退、无变更重放、断连后的任务排空和未知 scheme。

渲染迁移回归使用 `render_controls` 与 `editor_render_acceptance`，覆盖配置持久化、实时深度切换及重启恢复、离屏调试、缺失特性与匹配负载的合批比较。`editor_render_controls` 验证 GUI 切换及冻结相机保持；`deferred_rendering` 包含同一 session/swapchain 的离屏深度反复切换、像素和资源退休检查。
