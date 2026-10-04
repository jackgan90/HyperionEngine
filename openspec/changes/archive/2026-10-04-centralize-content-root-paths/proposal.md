## Why

Content、导入验证和 Editor 重复维护 `/Game` 根身份及路径边界判断，Browser 还用固定长度截取前缀。将领域规则归到 Content 并复用既有 GUI 展示工具，可以减少语义漂移，同时保持现有文件系统和错误契约。

## What Changes

- Content 提供 Game/Engine 根常量与按路径分隔边界识别包根的查询；保留大小写与原始输入语义。
- Content 提供基于已有 root snapshot 的未设根检查；导入与 automation 在原有校验位置转换为各自异常类型并保留提示。
- Editor 的目录默认值、导入输出构造、索引筛选和路径展示复用这些定义；移除 Browser 的重复前缀截取，使用 FGuiPathDisplay。
- 保持 operation/schema、请求默认值、错误 code/message/顺序、挂载生命周期和 GUI 行为。

## Capabilities

### New Capabilities

- `content-root-path-contracts`: Content 拥有根身份及词法路径归属，消费方复用领域检查并保留 IO 与 GUI 职责。

### Modified Capabilities

无。用户能力和外部协议不变。

## Impact

涉及 Runtime/Content、AssetImport、Editor 和 Automation 适配器。IO 的规范化、大小写、链接和越界校验不变；Gui 保持通用、无 Content 依赖。新增内部 API，不新增模块或用户能力。配套精确错误/路径回归、协议快照、Editor 内容和导入验收；独立审计后停在用户验收点。
