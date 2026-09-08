# Instance batching 交付审计

2026-09-08，审查 `add-instance-batching` 相对基线 `145162a7a1332869fcd6fac9dbf4e2205a1a40f0` 的全部实现、测试、工具和文档。独立 reviewer 不继承实现会话，首次审查覆盖 78 个变更/新增文件；主 agent 复核并最小化修复后，原 reviewer 对 81 文件快照进行定向复审。

## Findings 与处置

| ID | 级别 | 主 agent 复核 | 修复与验证 |
| --- | --- | --- | --- |
| IB-01 | P2 | 确认：有效普通材质附带超出 native 常量绑定限制的可选 Instance permutation 后，整个材质在就绪阶段失败，无法进入普通回退。独立 probe 与主 agent 复现一致。 | 普通材质就绪阶段仅准备必需 pass；可选 Instance native 准备进入已有批次失败回退路径。新增真实 GPU 回归覆盖 15 个 VS 常量缓冲超限和仅 Instance 使用不兼容顶点输入的 PSO 失败，两者均保留可就绪普通材质、完整四项覆盖、正确回退计数及完全相同图像。 |
| IB-02 | P2 | 确认：Renderer 已统计回退原因，但 Viewer GUI/CSV 未消费，遗漏 diagnostics 需求。 | GUI 显示非零回退原因；CSV 在原有字段后追加六类原因计数。真实移动相机 A/B 检查字段及 disabled 覆盖，实际 GUI/CSV 对照验证 194 普通 draw / disabled 194 与 5 instance draw / disabled 0。 |
| IB-03 | P3 | 确认：`--no-instance-batching` 强制普通路径，但 GUI 仍显示可操作且勾选的开关。 | 强制关闭时显示明确的 off 与 CLI 参数文本；正常启动保留原 checkbox。最终 Release GPU 截图直接核验两种状态，均通过 GUI readback 验证。 |

原独立 reviewer 已复审关闭三项，未发现新增确认缺陷或遗留阻断项。修复没有重设模块职责、修改持久化格式或扩展到其他 native 后端。

## 原始证据

- 首次独立报告：[Review.md](../out/InstanceAudit/Review.md)；定向复审：[ReReview.md](../out/InstanceAudit/ReReview.md)。
- 审查快照：[首次 78 文件](../out/InstanceAuditSnapshot.json)、[修复后 81 文件](../out/InstanceAuditFixedSnapshot.json)。复审期间文件哈希保持不变；交付报告和 OpenSpec 同步/归档随后完成。
- IB-01 主 agent 修复前/后 probe：[Before](../out/InstanceAudit/MainOptionalNativeBefore.log) / [After](../out/InstanceAudit/MainOptionalNativeAfter.log)；新增回归在运行时修复前失败：[RegressionBefore.log](../out/InstanceAudit/RegressionBefore.log)，最终针对性测试通过：[RegressionAfter.log](../out/InstanceAudit/RegressionAfter.log)。
- GUI：[强制关闭](../out/InstanceAudit/ForcedOrdinary.png) / [默认开启](../out/InstanceAudit/BatchEnabled.png)；同名 CSV 各包含 40 个采样帧，visible 为 194、failed 为 0，六类原因计数已逐行核验。

这些 `out/` 文件为本机原始日志、截图和快照，不纳入 Git。可重跑的 regression shader、C++ 测试及集成检查随源码提交。

## 修复后最终验证

| 验证 | 结果 | 本机日志 |
| --- | --- | --- |
| Debug 构建与完整 CTest；RenderDoc 开、Tracy 关 | 46/46，105.03 s | [DebugVerified.log](../out/InstanceAudit/DebugVerified.log) |
| Release 构建与完整 CTest；RenderDoc、Tracy 关 | 41/41，40.12 s | [ReleaseVerified.log](../out/InstanceAudit/ReleaseVerified.log) |
| Profile 构建与完整 CTest；RenderDoc 关、Tracy 开 | 41/41，77.82 s；其中 trace 验收 26.20 s | [ProfileVerified.log](../out/InstanceAudit/ProfileVerified.log) |
| 路径、格式、语义命名 | 268 源文件 / 166 编译单元，通过 | [FinalNaming.log](../out/InstanceAudit/FinalNaming.log) |
| 模块边界 | 261 源文件 / 25 模块，通过 | [Boundaries.log](../out/InstanceAudit/Boundaries.log) |

GPU 验证使用 Windows、RTX 5080、D3D12 debug layer。实例测试覆盖 typed record 打包、真实像素一致性、容量/extent 拒绝、兼容性与排他覆盖、缓存失效、跨 view、整组失败修复和旧帧资源保留；移动相机集成验证像素与覆盖一致。最终 [71 个 source/build/tool 文件哈希](../out/InstanceAudit/VerifiedSourceManifest.json)对应复审后的内容。

独立 reviewer 自行运行了初版实例 GPU suite 和 native 限制 probe，逐项检查修复后的源码、回归日志、完整 suite 汇总、截图及 CSV；未独立重跑全部 suite。没有执行 native Vulkan/Metal、系统性 OOM/device-loss 注入或人工点击 GUI。GUI 两种启动状态已用实际 GPU readback 验证。

[性能报告](InstanceBatchPerformance.md)中的重复长采样及 CPU/GPU trace 对应其中记录的审计前 exe；审计后完整正确性 suite 已重跑，但未重做长时间性能测量。

## OpenSpec 收尾

全部 12 项任务及四类 artifacts 已完成，11 项需求已同步到三个主规范，并归档为 [2026-09-08-add-instance-batching](../openspec/changes/archive/2026-09-08-add-instance-batching/implementation.md)。归档后严格校验为 31/31，活动 change 为空；见 [PostArchiveValidation.log](../out/InstanceAudit/PostArchiveValidation.log)。最终提交范围为 85 个源代码、测试、工具、文档与规范文件，生成物留在 `out/`。
