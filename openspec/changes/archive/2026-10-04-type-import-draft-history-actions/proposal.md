## Why

导入草稿的 Undo/Redo/Reset 在 GUI、原生调用和共享服务间以字符串表达语义。将领域动作类型化能消除原生调用的拼写协议，同时必须保留现有反射输入对未知字符串的处理及错误优先级。

## What Changes

- AssetImport 定义草稿历史动作枚举，GUI 与原生调用使用类型化入口。
- 既有 FImportDraftHistory 保留为字符串协议请求；由同一领域服务边界解析，委托给唯一的历史实现。
- 保持操作 ID、反射 schema/default、archive/wire 字符串、异常类型/码/文字、校验顺序、generation 和无发布副作用。
- 补充原生与 JSONL/MCP 历史操作、非法请求与旧格式回归，并记录该边界的用途。

## Capabilities

### New Capabilities

- `typed-import-draft-history`: 草稿历史动作的领域类型及兼容字符串协议边界。

### Modified Capabilities

无。现有用户功能及协议不变。

## Impact

涉及 Runtime/AssetImport、Editor 导入预览调用点和相关测试/文档。无需新增模块、插件生命周期、反射功能或 transport 分支。灯光诊断、截图目标、关闭状态观察和验收选项另行推进。
