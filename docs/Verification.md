# Windows baseline acceptance

2026-09-06：九个 OpenSpec changes 按顺序实施、验证并归档。实际运行环境为 Windows x64、MSVC、NVIDIA GeForce RTX 5080，D3D12 调试层已启用。

| 检查 | 结果 | 证据 |
|---|---|---|
| Debug 构建 / CTest | 13/13 通过 | `out/change09-debug.log` |
| Release 构建 / CTest | 13/13 通过 | `out/change09-release.log` |
| 可见窗口运行 | 180 帧；GPU 验证错误 0 | `out/visible-launch.log` |
| 三角形与 GUI 截图 | 像素验收通过，已目视检查 | `out/captures/final.png` |
| 多线程与配置重启 | 四类执行域、保存恢复、插件禁用/错误 ID | `out/build/release/viewer-acceptance` |
| GUI 输入与反射 | 鼠标操作、参数保存、绘制数据所有权通过 | `tests/GuiTests.cpp` |
| 跨平台 shader 工具链 | DXIL / SPIR-V / MSL 生成通过 | `tests/ShaderTests.cpp` |
| SPIR-V 附加验证 | GUI 两个 stage 通过 Vulkan 1.1 验证器 | `out/build/debug/shader-test/gui-*.spv` |
| 第三方隔离 | include 边界检查通过 | `tools/CheckBoundaries.py` |

CTest 包含 core、tasks、configuration_plugins、assets_math、shaders、render_graph、gui_input_and_data、dependency_boundaries、window_lifecycle、d3d12_clear、d3d12_triangle、d3d12_gui 和 viewer_acceptance。

产物、依赖缓存和日志均放在被 Git 忽略的 `out` 下。重新运行 README 中的构建/测试命令即可生成。每个 change 的设计、规范、任务完成状态和验证说明保存在 `openspec/changes/archive`。

Vulkan/Metal 实际后端、移动平台、多目标 Render Graph、动态插件更新和全局分配拦截不在本次验收范围内。

## UE 风格迁移验证

2026-09-06：`adopt-unreal-code-style` 完成 52 个自有文件的命名迁移，更新全部公共接口、实现、shader、测试和构建引用。默认约定见 [代码规范](CodingStyle.md)。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| VS 2022 Debug / CTest | 14/14 通过，含 DX12 三角形、GUI、窗口与配置验收 | `out/style-vs-debug.log` |
| VS 2022 Release / CTest | 14/14 通过 | `out/style-vs-release.log` |
| Ninja Debug 构建 | 通过；用于更新 clang-tidy 编译数据库 | `out/style-ninja-debug.log` |
| 默认格式与命名检查 | 40 个 C++/HLSL 文件、24 个 C++ 翻译单元通过 | `out/style-naming-check.log` |
| 检查器反向验证 | 错误文件名、include 大小写、排版、命名均被拒绝；探针已移除 | `out/style-negative-checks.log` |
| 一键生成入口 | 从仓库外目录成功重新生成 `.sln` | `out/style-cmd.log` |
| 迁移审计 | 依赖 lock 和实验 JSON 数据不变；运行时字符串仅变更自有文件路径 | `out/style-migration-audit.log` |

CTest 在原有 13 项基础上新增不依赖 LLVM 的 `code_style_paths`。完整格式和语义命名检查仍通过 `python tools/CheckStyle.py --naming` 显式执行。MSVC 的剩余警告来自第三方 tinyexr；自有代码未产生编译警告。
