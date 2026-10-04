## Why

Editor 的导航、拾取、放置、Gizmo、层级手势、快捷键、文档和附加窗口分别组合模态与交互事实，新增状态容易漏改入口。本次收敛规则归属，保持已确认的入口差异和共享文档最终校验。

## What Changes

- 在 Editor 私有领域集中采集即时交互事实，并通过独立命名策略查询决定各入口的 admission。
- 保留 popup 与文档 busy、Gizmo 与导航、关闭保存与附加窗口之间的差异；保留取消、焦点恢复和事务收尾路径。
- 用策略矩阵及现有真实 Editor 验收覆盖阻断、解除阻断和同帧状态变化。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `editor-application-consolidation`: 明确 Editor 交互事实和分入口策略的单一归属及兼容性要求。

## Impact

限于 Plugins/Editor 私有输入与文档适配、测试和文档。SceneEditing 仍拥有修改、history、revision 与持久化校验；不新增 automation operation，不修改插件选择或生命周期。
