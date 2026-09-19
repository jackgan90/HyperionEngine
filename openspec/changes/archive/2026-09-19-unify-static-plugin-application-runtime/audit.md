# 插件架构独立审计

## 审查版本与范围

基线为 `67e577f03c70f1d252c4fc1b926abab29a25ea63`，目标为本变更的未提交工作区，含新增、修改和删除文件。初审冻结 133 项路径，暂存区为空，无已知无关用户改动。冻结清单为生成物 `out/PluginAudit20260919/InitialSnapshot.json`，SHA-256 为 `45ff2193bdeeba60f735c96697c4b4e5fa4920d70af6f1c38ed5a48339748913`。

依照 `.codex/skills/quality-audit/SKILL.md`，三个不继承会话上下文的独立 reviewer 分别检查插件运行时与宿主、应用组合与服务所有权、渲染与 GUI 扩展。主 agent 独立核对调用链、基线行为和复现证据。初审结束后重新核验全部路径，确认审查期间未修改源码；随后才实施下述局部修复。

用户补充重点已覆盖：AGENTS.md 强制要求阅读 PluginSystem.md，六条开发约束及 SourceLayout/文档索引入口明确规定最小宿主、可选性、生命周期、跨插件通信、线程边界和验证责任。

## Findings 与处置

| ID | 优先级 | 复核证据 | 处置与复审 |
|---|---|---|---|
| RT-01 | P2 | PluginServices.cpp 每次发布复制 callable，mutable 值捕获计数三次均为 1，预期为 1、2、3 | 回调采用稳定存储，调用期间保活，保留订阅 ID 快照和移除检查。新增重入、状态持久性及订阅增删测试。原 reviewer 重编探针得到 1、2、3，关闭。 |
| RT-02 | P2 | Strict 请求有效插件后再请求缺失插件时，已执行一次 Start；基线先完成完整依赖检查，当前 OpenSpec 也要求激活前检测 | Strict 在计划阶段拒绝已知不可用项，任何 factory/Start 均未执行。覆盖缺失插件、依赖、服务及显式禁用；Continue 行为保留。原 reviewer 探针启动计数为 0，关闭。 |
| RT-03 | P2 | 失败 Start 的 cleanup 抛错后，原启动诊断存在，但最终 GetStopFailure 为空；失败 factory 同样遗漏清理异常 | 所有启动失败分支显式清理作用域并保留首个清理错误。覆盖 factory/Start 两条路径、继续清理、退订、无关插件继续运行及宿主最终报告。原 reviewer 确认 stop-failure=1 且原启动诊断保留，关闭。 |
| R-S1 | P3，文档建议 | SceneViewer 的 Update 已推进相机，公开注释仍要求外部每帧调用 AdvanceCamera | 仅更新注释，明确手动推进适用范围；渲染 reviewer 对照 Update 调用链复核，关闭。 |

复审快照为 `out/PluginAudit20260919/RepairSnapshot.json`，SHA-256 为 `7c3ef270f0aed13b912c1cf8328b2d6733a49c8beb45639f7ce08942f2ad5872`。运行时 reviewer 核验 8 项修复相关文件，独立重编旧探针并检查新增回归测试，未发现修复引入的新确认问题。

## 初审验证建议与边界

**APPS-N1：停机末段 GPU validation 检查覆盖，后续已关闭。** 初审发现 Viewer/Editor 在自身 Stop 中读取 validation 计数时，GUI/scene provider、RenderSession 和 Swapchain 尚未全部释放，graphics provider 清理后未再次检查。初审将其保留为验证建议；用户随后授权实现与测试，停机末段错误注入证实检查缺口，修复与独立复审见下节。此证据证明错误报告覆盖不足，不代表正常运行已出现真实 GPU 生命周期错误。

本轮没有重新执行完整 81 项回归、编译裁剪矩阵、RenderDoc 捕获或交互式最小化/保存验证；此前结果见 validation.md，不计为本轮重新验证。渲染 reviewer 的结论基于源码及资源依赖/保活追踪，不代表任意未来第三方 feature 均已通过 GPU 验证。

## 本轮实际验证

- 修复前：现有 configuration_plugins、plugin_runtime、dependency_boundaries、code_style_paths 共 4/4 通过；独立探针仍复现三个缺陷，说明原覆盖不足。
- 应用 reviewer 补充 6 项串行短程验收：陈旧双 source 与禁用 GUI 的 Triangle 截图成功，8 帧且 validation=0；未注册 backend、禁用 scene 后保存、禁用 Viewer 后截图、禁用 Editor GUI 后截图、缺失 mounts 均按预期受控失败。
- 修复后：Visual Studio Debug 的 plugin_tests、config_tests、Viewer、Editor 构建成功；configuration_plugins、plugin_runtime、plugin_applications、editor_acceptance、viewer_acceptance、dependency_boundaries、code_style_paths **7/7 通过**。
- 全部 612 个自有源码文件路径和格式通过；修复的 4 个 C++ 翻译单元语义命名通过；OpenSpec strict validation 和 git diff --check 通过。
- 源码与 OpenSpec 保留在工作区；未暂存、未提交、未归档。

原始探针和日志位于忽略的 `out/PluginAudit20260919/`，包括 InitialSnapshot.json、RepairSnapshot.json、BaselineChecks.log、RepairBuild.log、RepairTests.log、RepairNaming.log 及 Runtime/Apps/Render 子目录；不要求新检出仓库保留这些本地生成物。

## Graphics 最终检查后续修复与复审

本项仅检查上述插件改造之后的局部差异，基线仍为同一 HEAD；实现前文件副本及哈希保存在 `out/GraphicsShutdownAudit/Before/` 和 `Before.json`。最终复审冻结 11 个实现、测试、构建和契约文件，清单为 `RereviewSnapshot.json`，SHA-256 为 `2b6f636689da4f64b1059544ac5b06107187fe474fb7ee5c41a48505774ae952`。独立 reviewer 不继承主会话上下文，审查期间源码保持冻结，起止 11/11 文件哈希一致。复审后只更新任务勾选与审计、验证记录。

| ID | 复核与修复 | 独立复审 |
|---|---|---|
| APPS-N1 | 在真实 D3D12 InfoQueue 中，于 Swapchain 实现销毁后注入一条 ERROR；较早统计为 0，原实现无法向宿主报告该错误。graphics 现在关闭消费者和 RenderSession 后，在 RHI 0 等待 GPU、释放 Swapchain、读取统计并释放 Device，再由 Main 向 FApplicationControl 报错。 | 初审确认检查位置、错误传播和设备释放；新增测试依次覆盖正常、注入错误、正常及重复 Stop。关闭。 |
| GS-EXISTING-001，P2 | 本项初审发现原有 WaitIdle 抛错路径把仍存于插件成员中的 Swapchain/Device 留到 Main 析构。主 agent 使用已有假后端复现，测试在 RHI 销毁线程断言失败。两项所有权现在于 WaitIdle 前移入 RHI 局部变量，异常展开按 Swapchain、Device 顺序释放。 | 原 reviewer 独立运行等待失败用例，核实错误报告、两次析构的线程与顺序、重复 Stop，确认修复且未发现新增确认缺陷。关闭。 |

实际验证：

- 两项新增测试均保留修复前失败、修复后通过的证据。晚到错误用例的最终日志依次为 validation 0、1、0；注入错误时宿主收到精确失败信息，设备状态已释放。
- Visual Studio Debug 构建 `rhi_contract_tests`、`d3d12_frame_failure_tests`、Viewer、Editor 成功。最终 CTest **7/7 通过**，58.19 秒：graphics_shutdown_validation、graphics_shutdown_wait_failure、rhi_backend_contracts、d3d12_frame_failure_recovery、plugin_applications、editor_acceptance、viewer_acceptance。
- 612 个自有源码文件路径/格式、4 个改动翻译单元语义命名、579 个源码文件/35 个模块依赖边界通过；OpenSpec strict validation 和 git diff --check 通过。
- 原生注入钩子只编译进现有 D3D12 故障测试目标；生产后端未增加注入分支或公共接口。假后端测试新增的私有 ApplicationServices 链接不引入 native backend。
- 相对本项开始时的快照，生产 GraphicsServices 为 **+22/-5 行**；测试和仅测试启用的钩子为 +161/-1 行，CMake 为 +7/-1 行。测试增量包含复审确认的既有等待异常回归。

本次没有重跑完整回归、裁剪构建或 Release。真实设备移除和 Statistics 抛错未动态注入；后者的 RHI 清理由局部所有权展开复核。最终统计读取发生在 Device 析构前，不覆盖其自身析构期间才产生的消息。任务入队前的分配/接纳失败和更早 RenderSession 的异常处理未在本项扩展。当前审查范围内没有未关闭的确认 finding。

原始证据均为忽略的本地生成物：`out/GraphicsShutdownAudit/` 下的 BeforeFixTest.log、WaitFailureBeforeTest.log、FinalBuild.log、FinalTests.log、FinalTestDetails.log、FinalNaming.log，以及 Review/IndependentReview.md、Review/Rereview.md。实现、测试和 OpenSpec 保留未提交，暂存区为空。
