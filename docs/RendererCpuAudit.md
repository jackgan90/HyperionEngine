# CPU 渲染优化的独立审计修复

审查范围是 `optimize-renderer-cpu-submission` 与 `optimize-retained-render-frames` 的全部工作区改动。基线 commit 为 `144c1e5e4c60816f15f9ecc9e520e9c9d2002d21`。本轮开始时逐一核对了审计快照的 117 个文件，当前内容均匹配；原快照摘要为 `28a1e54c294a30ead6fbcc6c802ce1cd507a3f69bf4aa46bc88c4f7fd27b7a3b`。

原始报告、probe、日志和源码快照保留在 `out/audit-cpu-proposals-20260909`。材料组 `Review.md` 与原生组 `InitialReview.md` 已核对；原来没有最终报告的 scene-graph 组补做独立核查并生成 `Review.md`。修复后的结果不会覆盖原始失败证据。

## 已确认问题与修复

| ID | 影响 | 修复与回归覆盖 |
| --- | --- | --- |
| MAT-01 / P2 | 失效批次计划的完整参数表长期持有旧 CPU texture/buffer source | 普通 family 退休删除 emitted owner 失效的计划并更新预算；scene invalidation 清理计划，即使没有下一帧。测试检查旧帧仍保有原值、随后资源释放及其他活跃计划复用。 |
| NATIVE-01 / P1 | Shared/inline owned command 只有一份嵌套 buffer 句柄时，reset 错误通过，能改写已录制或 GPU 在途的常量 | 在成功验证前登记弱 stream/command owner，页锁串行化登记、发布、检查和 reset；reset 同时检查实际句柄与使用凭据。共享和内联测试都构造 `use_count()==1`，验证录制中及真实 fence 未完成时拒绝、所有 owner 释放后允许。 |
| NATIVE-02 / P2 | 已预热 native plan 接受同时存在 SharedDraws 和 Draws 的非法 shell，静默忽略一部分输入 | 每次查缓存前校验 `GetDraws()` 存储不变量；回归覆盖预热后非法混合输入。 |
| SG-01 / P1 | 延迟 scene/depth/GUI graph 在 owner 销毁后访问悬空对象 | Scene/depth 捕获拥有 coordinator 的 preparation 句柄，GUI 使用存活状态凭据；关闭或销毁后的执行在 BeginFrame 前拒绝，原 immediate API 保持可用。 |
| SG-02 / P2 | 复用后 LastDraw 的 frame/view 在视图换序或移除时过期 | 每次复用按当前 family/view 顺序发布成功回执；多视图换序、缩减到单视图的回归同时检查 packet reuse 和 receipt。 |
| SG-03 / P1 | 首次 Close 失败已释放 MaterialState，重试时解引用空状态 | 失效处理允许空 MaterialState，继续清理计划与资源；测试注入一次 WaitIdle 异常并验证后续 Close 和析构。 |

MAT-02 是原报告标注的有界 overlay 隐藏资源保留观察，未造成错误取值；按原审计范围保留为后续项。本轮没有开展新的移动相机性能优化或材质表示重构。

## 验证记录

本轮结果及修复文件哈希保存在 `out/audit-cpu-proposals-20260909/fixes`。三组针对性复审均关闭原 findings，没有新增已确认缺陷：

- [材料复审](../out/audit-cpu-proposals-20260909/materials/ReReview.md)：另独立执行真实场景移除、外部旧 graph 保留、三视图预算回收及 item/chunk 保留 probe。
- [原生复审](../out/audit-cpu-proposals-20260909/native/ReReview.md)：另独立执行 cold mixed、首次登记前 stale slice、跨 recording context 登记、取消后外部 recorded owner 与最后释放 probe。
- [场景和 graph 复审](../out/audit-cpu-proposals-20260909/scene-graph/ReReview.md)：另独立执行重建后的 CPU RHI contract tests，区分本轮主 agent 执行的 GPU 结果。

Debug、Release、Profile 完整构建均通过。每种配置运行相同的 10 项相关 CTest，均为 **10/10**：`material_rendering`、`render_graph`、`cascaded_shadow_rendering`、`instance_batching`、`render_resources`、`scene_rendering`、`rhi_backend_contracts`、`d3d12_device_ownership`、`d3d12_frame_failure_recovery`、`d3d12_gui`。最后增强的三视图预算测试在三配置构建中均已包含；Debug 另增量构建并重跑 `instance_batching`，**1/1** 通过。

GPU 测试和独立 probe 保持 D3D12 debug layer 开启、validation errors 为 0。检查了本轮 GUI 截图，三角形、诊断控件及裁剪正常。GUI 的 CPU owner 生命周期测试使用未真实 Start 的 fake device，正常 Start/绘制由 `d3d12_gui` 覆盖；未据此声称覆盖所有 plugin reload 交错。

路径/格式、184 个 translation unit 的语义命名、模块边界检查通过。最终增强测试另做针对性命名检查通过；OpenSpec strict 为 **35/35**，tracked diff 和 29 个新增文件的 whitespace 检查通过。`ChecksVerified.log` 确认全部 24 个修复源码/test/CMake 文件与独立复审版本一致（包含最后增强测试的复审哈希）。完整命令、日志和最终候选哈希保存在上述 `fixes` 目录。

## 修复的性能代价

保存审计前 Release/Profile 的 Viewer、native benchmark 和运行库，SHA-256 见 `fixes/BaselineBinaries.json`。Release 对照使用同一设备和验证设置、Tracy 关闭、VSync 关闭；每个负载预热 200 帧、采样 400 帧，做两轮反向交替 A/B。600/1200 ordinary draw 与默认 Forward/CSM 的静止/移动共 32 次运行，12,800 个采样帧全部通过就绪、逐帧 GPU 提交、实际 draw/source 覆盖和资源稳定性检查。

下表合并两轮的全部样本，单位为毫秒；箭头为审计前 → 修复后。两轮短测仍有调度和 GPU/Present 等待波动，不足以将每个整帧差值精确归因到某一修复。保留所有回退和离群样本。

| 负载 | 整帧均值 | 整帧 P95 | Pipeline 准备均值 |
| --- | --- | --- | --- |
| 600 静止 | 2.374 → 2.360 | 4.378 → 4.452 | 0.023 → 0.054 |
| 600 移动 | 3.387 → 3.436 | 4.299 → 4.348 | 1.757 → 1.746 |
| 1200 静止 | 2.887 → 2.791 | 4.400 → 4.360 | 0.027 → 0.097 |
| 1200 移动 | 7.323 → 7.625 | 8.216 → 8.521 | 4.446 → 4.632 |
| 默认 Forward 静止 | 2.185 → 2.271 | 5.169 → 4.854 | 0.026 → 0.039 |
| 默认 Forward 移动 | 2.334 → 2.366 | 4.447 → 4.625 | 0.585 → 0.597 |
| 默认 CSM 静止 | 2.435 → 2.784 | 4.719 → 5.145 | 0.044 → 0.083 |
| 默认 CSM 移动 | 3.246 → 3.182 | 4.868 → 4.841 | 1.752 → 1.719 |

回执修复恢复了缓存命中时按 item 发布诊断的工作；静止 600/1200 的准备均值分别增加约 0.031/0.070 ms，默认静止 CSM 增加约 0.039 ms。不能继续使用审计前“每帧只写单个时钟、避免逐 item 回执”的描述。本轮没有承诺所有负载零回退，1200 移动和静止 CSM 的整帧回退也如实保留。

原生 benchmark 另按 borrowed/owned × A/B × 两轮执行 8 次，覆盖 0/1/100/300/600/1200，每组预热 100、采样 200，单独计量 Record 而不包含 Present/BeginFrame 等待。600/1200 owned 的均值为 **0.6794 → 0.6912 / 1.2797 → 1.2638 ms**；borrowed 为 **0.7459 → 0.7664 / 1.4223 → 1.4345 ms**。所有运行 validation errors 为 0。该 native 工具按请求的 geometry 数分类；独立的实际覆盖验证来自完整引擎 benchmark 和正确性测试。

原始命令、每轮样本、均值/P50/P95/P99 和二进制身份见 [完整引擎结果](../out/audit-cpu-proposals-20260909/fixes/performance-release/Summary.json)、[原生结果](../out/audit-cpu-proposals-20260909/fixes/native-performance/Summary.json) 和 [聚合数据](../out/audit-cpu-proposals-20260909/fixes/PerformanceAggregate.json)。这些是本次正确性修复的 Release 性能复测，未重跑 Debug/Profile 的完整性能矩阵。

原有完整 0/1/100/300/600/1200 扫描、长时间运动、可见 UI 与 RenderDoc 证据仍属于审计前版本。未进行驱动级设备移除/OOM、分配失败注入或 sanitizer 验证。
