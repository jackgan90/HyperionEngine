# CPU 异步帧实现质量审计

日期：2026-09-10。基线 commit：`cca47b26c11476464c0dc7a58d15016f71debc39`。

## 范围与方法

审查当前未提交的 41 个候选文件，包括两级 CPU 帧管线、Viewer 集成、按帧统计与输入所有权、配置／测试／文档，以及后续默认值改为 1/1 和 `RenderScene.cpp` 两处局部变量遮蔽警告修复。没有排除的已知无关改动，索引为空。

按 `quality-audit` 流程，首次 reviewer 不继承实现会话上下文，只接收任务要求、基线、候选清单和原始验证入口。Reviewer 审查期间主 agent 保持候选不变，并独立核查直接调用链。初始清单和每文件 SHA256 为 `out/QualityAuditCpuFrames/Candidate.json`；文档修正后的清单为 `out/QualityAuditCpuFrames/CandidateFinal.json`。

## 发现、复核和处置

| ID | 优先级 | 主 agent 复核 | 修复与复审 |
| --- | --- | --- | --- |
| CPU-01 | P3 | 确认。`README.md:70` 仍写两级默认均为 0，和 `AppSettings.h`、`FramePipeline.h` 的 1/1、配置测试及原生默认启动日志不一致 | 将 README 改为默认均为 1；原 reviewer 独立核对并关闭 |

本轮没有修改 C++ 实现。两份候选清单相比仅 README 哈希变化，其余 40 项保持一致；复审再次确认全部 41 项哈希匹配。未发现其他可确认的并发、所有权、生命周期或规范缺陷，没有未解决的确认 finding。

## 重点覆盖

- Main 等待至 N−a、Render 等待至 N−b 的边界，显式零值、混合值、最大 16、默认 1/1、按序推进和失败停止接收。
- RHI 0 的同帧 peer 汇合、未参与线程、异常回收，以及 CPU 完成与 GPU fence 完成的区别。
- 帧 ticket 发布、Main 泵送与重入限制、按值输入／不可变快照、独立准备结果、缓存写时复制及资源发布锁。
- 跳过绘制的 tick、Resize、正常／异常排空与关闭，截图设置和就绪状态、RenderDoc 目标帧、benchmark submission identity、有效参数显示与 profiling 边界。
- 配置／CLI 校验、构建与回归入口、OpenSpec 和说明文档，以及 `PrimitiveRevision` 局部重命名。

## 实际验证

独立 reviewer 执行：

```powershell
ctest --test-dir out/build/debug -R 'cpu_frame_|configuration_plugins' --output-on-failure
```

结果 **5/5 通过，12.66 秒**：配置、CPU 限流／错误／排空、延迟两图所有权、原生 Viewer、连续 RenderDoc 目标帧抓取。默认启动日志确认 `leads=1,1`，80 个 CPU/GPU 帧完成、D3D12 validation errors 为 0。

主 agent 另检查当前 335 个源文件的格式与路径，以及最后修改的 `RenderScene.cpp` 编译单元语义命名，均通过。候选哈希、`git diff --check` 和本 change 的 OpenSpec strict 通过。README 的一行修正不改变程序行为，复审未重复原生测试。

原始记录：

- [独立审核报告](../../../../out/QualityAuditCpuFrames/Reviewer/Review.md)
- [独立针对性复审](../../../../out/QualityAuditCpuFrames/Reviewer/ReReview.md)
- [独立 Debug 测试](../../../../out/QualityAuditCpuFrames/Reviewer/TargetedDebug.log)
- [格式检查](../../../../out/QualityAuditCpuFrames/Style.log)与[局部重命名检查](../../../../out/QualityAuditCpuFrames/RenderSceneNaming.log)

## 验证边界

本轮审计使用已有 Debug 构建，没有重新执行完整 53 项套件，也没有在默认值改为 1 后刷新并重跑 Release/profile 或重新导出 Tracy。没有执行 ThreadSanitizer，也未增加分配器、操作系统或原生设备故障注入。此前构建与全套结果见 [实施记录](implementation.md)，不能当作本轮独立复现的结果；静态复核及定向测试不穷尽所有调度交错。

审计阶段未执行 OpenSpec 归档、Git 暂存或提交；后续按用户要求于 2026-09-10 同步主规范并归档。
