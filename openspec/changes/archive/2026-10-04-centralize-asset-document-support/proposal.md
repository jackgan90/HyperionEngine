## Why

Editor workspace 和独立 automation 分别维护同样四类基础资产文档的支持列表，容易产生 GUI 与 agent 差异。基础支持判断应由共享 CPU AssetEditing 领域提供。

## What Changes

- AssetEditing 提供基于规范反射类型身份的基础文档支持查询。
- Editor 和 automation 复用查询，并保留现有错误、加载/失败页签和各自预览限制。
- 覆盖四种支持类型、scene/未知类型拒绝及 GUI/automation 回归。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `shared-asset-documents`: 集中基础文档支持规则，明确其与预览能力的边界。

## Impact

Runtime/AssetEditing、Editor/Automation 的打开适配和测试文档。不改变文档事务、history、保存、operation ID、schema 或生命周期。
