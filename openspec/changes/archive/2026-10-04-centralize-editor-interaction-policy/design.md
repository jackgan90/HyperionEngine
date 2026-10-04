## Context

基线 13c997a 中阻断条件分散于 EditorFrame、EditorPicking、EditorPlacement、EditorGizmo、EditorStructure、EditorShortcuts、EditorSceneDocument 和 EditorAssetWindow。快捷键已有命令策略，文档和窗口已有独立领域服务。

## Goals / Non-Goals

**Goals:** 集中状态采集，明确各入口差异，保持交互、事务和取消语义。
**Non-Goals:** 不新增全局 busy，不重写手势状态机，不把 GUI 状态带入 Runtime/SceneEditing，不改变 automation 契约。

## Decisions

1. Editor 私有纯值 facts 与命名策略拥有输入 admission；FEditorPlugin 在每次查询时从当前 GUI、Viewport、手势和 Transition 采集，避免一帧缓存对同帧弹窗/手势变更过期。与把状态放入全局 service 或按帧缓存相比，此方案保持现有 Main 调用时序。
2. 相机、拾取、放置、Gizmo/overlay、层级开始/继续、快捷键、文档和附加窗口分别查询。相机阻断任意 pending reparent，Gizmo 只阻断 dragging reparent；普通 popup 仅进入快捷键所有权，文档 busy 保持旧集合；偏好弹窗不扩大旧 Gizmo 策略。命令层 FEditorShortcutInteraction::Allows 继续负责 Save/History/Clipboard 等差异。
3. 保留 callsite 对有效相机、选中节点、revision、bounds 和 payload 的校验，以及既有 SuspendInput、FinishGizmo、CancelPlacement、CancelReparentGesture、click reset。SceneEditing 的 RequireIdle/事务校验仍是最终领域权威。
4. 关闭 admission、场景面板开关可复用对应命名查询；异步保存、import、asset workspace busy 仍由各自领域提供，不合并生命周期。
5. 用独立表格测试逐项阻断与允许的差异，复用真实 Editor 的快捷键、Gizmo、拾取、放置、reparent、framing、content-transition 和资产窗口测试。不为内部策略新增外部操作。

## Risks / Trade-offs

- [条件误合并] → 以当前源码为基线记录矩阵，验证 prefs、popup、saving-close、pending inspector 和 pending/dragging reparent。
- [同帧状态过期] → 每入口即时采集，不存长期快照；维持原有输入批次分析。
- [已有超长类头] → 仅增加策略声明并改分类字段；本轮不展开无关 FEditorPlugin 所有权拆分，在验证记录中注明。

## Migration Plan

先实现纯策略和事实采集，再迁移入口并跑 Debug/Release 与独立审查。无数据迁移；回滚为恢复本变更。验收后另行授权归档和提交。

## Open Questions

无。
