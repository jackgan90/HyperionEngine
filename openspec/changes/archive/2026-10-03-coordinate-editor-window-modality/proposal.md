## Why

主 Editor 的未保存确认绘制在主窗口内，原生附加窗口会遮住确认。逐个宿主反转 SDL owner 的临时修复无法组合多个窗口，并破坏主窗口最小化时的附加窗口显隐。

## What Changes

- Platform 提供主窗口加任意数量直接附加窗口的作用域注册与统一模态协调。
- 保持主窗口和 SDL ownership 稳定，以私有原生适配协调输入、Z 序、最小化显隐和状态恢复。
- Editor 集中发布现有模态状态；Asset Editor 只接入注册，不再反转 owner。
- 覆盖多窗口、动态注册与注销、失败清理、重复进入退出、关闭保存/丢弃/取消。

## Capabilities

### New Capabilities
- `window-group-modality`: 作用域窗口组、组内主窗口模态优先级和生命周期恢复。

### Modified Capabilities
- `asset-editor-window`: 主窗口保护流程必须覆盖原生资产窗口，保持文档保护及最小化恢复行为。

## Impact

Runtime/Platform 的公共窗口组契约与私有 Windows 适配；Plugins/Editor 的窗口注册与帧同步；原生桌面测试与 Editor 自动化验收；插件文档。不改变 automation 操作 ID、反射 schema、保存事务、插件依赖或 Runtime/Application 职责。不归档，不提交。
