# Render primitive 独立审计记录

日期：2026-09-07。基线 `0b688a8f49026af3a1c524177eb64b009ed15dde`，审核本 change 的全部未提交变更，包含新增文件和删除项。按仓库 `quality-audit` 流程创建不继承会话上下文的 reviewer；主审独立复核、完成范围内修复，再由原 reviewer 复审。未提交 Git 或归档 OpenSpec。

## 结论和版本

初审覆盖 64 项文件，确认 2 项 P2；主审复核均成立并作局部修复。独立复审确认 **RP-01、RP-02 均关闭，未发现修复直接影响中的新增确认缺陷**。未发现需要改变已批准架构或扩大本 change 范围的问题。

初审快照为 `out/RenderPrimitiveAudit/InitialManifest.json`；修复快照为同目录 `FixedManifest.json`，仅 9 项文件变化。两个审查阶段结束时，reviewer 都重新校验了 64 项 SHA256，差异为 0。复审后仅补本审计记录和 verification.md，受审源码未再修改。最终清单为 `FinalManifest.json`。

## Findings 的复核与处置

| ID | 原问题与触发条件 | 主审复核、修复及复审 |
| --- | --- | --- |
| RP-01 / P2 | Ready 资源只有 section 0，两个 primitive 的更新 batch 中第二个引用 section 1。校验漏掉 section，两者均发布 revision 2，下一帧整体抛错。 | 主审核对状态校验→整批发布→场景收集，并独立重跑 probe，确认违背非法 batch 不发布契约。在发布前检查已知 section；pending 描述完成后的非法 section 由 binding 独立报告 Failed，并过滤无效 item。补整批拒绝、非法创建、无帧延迟诊断、其他对象继续绘制、纠正更新及移除/关闭回归。原 probe 现在拒绝更新、两者保留 revision 1、帧正常准备。Reviewer 已关闭。 |
| RP-02 / P2 | 无 RenderSession 的直接 RHI 客户端连续出帧。EndFrame 新设备级保活替代了 Frame.Retained，但 BeginFrame 未回收设备集合。 | 主审在真实 D3D12 上复现：12 帧 GPU 全部完成，仍保留 12 组提交。恢复 BeginFrame 既有 fence 等待后的非阻塞采集；不增加全局 WaitIdle，也保留 coordinator 无新帧退休入口。新增超过 frame-ring 长度的直接 RHI 回归。原 probe 现在完成 12 帧后仅保留最后一组提交。Reviewer 独立复核及重跑后关闭。 |

两项都由本次实现引入，属于已授权的功能与兼容性范围，没有更改公共接口或重写资源生命周期。延迟 section 校验不回滚先前已接收的 pending revision，后续合法更新/Remove 仍可处理；该边界已写入 `docs/RenderPrimitives.md`。

## 覆盖和验证

初审覆盖 Main/Render 所有权与继承边界、owned messages、batch/revision/generation、异步失败与迟到结果、共享资源和实例隔离、无新帧退休及关闭、全场景剔除/透明/深度、Viewer/插件迁移、直接 RHI 兼容性、模块边界及文档契约。实际 GPU 为 RTX 5080，D3D12 debug layer enabled。

以下结果均针对修复后的源码，本轮采用相关回归，没有把实现阶段的完整套件结果当成本轮重跑：

| 主审实际验证 | 结果 | 日志（均在 out/RenderPrimitiveAudit） |
| --- | --- | --- |
| Ninja Debug 全部目标构建 | 通过，无自有代码编译诊断 | NinjaBuild.log |
| Ninja Debug 相关 CTest | 8/8，24.23 s | NinjaTests.log |
| VS 2022 Debug 全部目标构建 + 相关 CTest | 通过，8/8，33.86 s | VsDebugBuild.log / VsDebugTests.log |
| VS 2022 Release 全部目标构建 + 相关 CTest | 通过，8/8，30.59 s | VsReleaseBuild.log / VsReleaseTests.log |
| 命名、局部声明、bool 前缀、格式/include 路径 | 74 编译单元、127 源文件通过 | Naming.log |
| 模块边界 | 123 源文件、23 模块通过 | Boundaries.log |
| OpenSpec change / all strict | 通过 / 21 项通过 | OpenSpec.log / OpenSpecAll.log |
| tracked / untracked 空白检查 | 通过 | DiffCheck.log / UntrackedWhitespace.log |

每组 8 项为：model_fixtures、render_primitives、render_resources、scene_rendering、model_rendering、d3d12_device_ownership、d3d12_frame_failure_recovery、model_viewer_acceptance。覆盖实际材质像素、共享模型、实例隔离、无帧退休、Present 失败恢复、相机和 Viewer 行为。

Reviewer 复审另行实跑修复后的 primitive/resource/scene/frame_failure/device 五个 Ninja Debug 测试及两份原缺陷探针，全部通过。初审时，直接运行 VS model_render 测试曾遇到原截图目标 `PNG write failed`；reviewer 保留已有文件，仅将测试副本的三个截图路径指向审计目录后完整通过。主审最终三组 CTest 的原 model_render 测试均通过。

原始独立报告：[初审](../../../../out/RenderPrimitiveAudit/IndependentReview.md)、[复审](../../../../out/RenderPrimitiveAudit/TargetedReReview.md)。复现源码、构建脚本、前后日志和哈希清单均在该审计目录。

本轮未重复完整 35 项 CTest 或 RenderDoc 捕获/回放；其实现阶段结果保留在 verification.md，不能视作此次实跑。未注入真实 device loss、内存分配失败或无限期压力负载。GPU instancing、动画/蒙皮等既定非目标未纳入验收。
