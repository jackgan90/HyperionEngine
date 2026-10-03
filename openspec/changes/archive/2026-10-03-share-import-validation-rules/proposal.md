## Why

Workspace 预检与独立原生发布重复维护输出扩展名、源/输出隔离、默认依赖库和来源标识规则，后续修改容易只覆盖一个入口。将这些现有规则收敛到 AssetImport 私有实现，降低规则漂移风险并保持用户可见行为。

## What Changes

- 提取共享的依赖库路径归一化、输出合法性和来源标识检查。
- Workspace、ImportAsync 和 PrepareAsync 复用各自已经执行的规则，保留入口专属策略、错误类型/文本和检查阶段。
- 增加两个公开入口的行为回归，覆盖非法输入、默认库、分组策略差异及失败零写入。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `editor-asset-import`: 明确 Workspace 与发布服务共享导入基础规则，并保持内容根准入和 GUI/automation 契约。
- `offline-asset-import`: 明确独立发布入口使用共享规则而不依赖 Workspace，保留发布和只读准备的既有准入语义。

## Impact

仅涉及 Runtime/AssetImport 私有实现、相关测试和文档，不新增公共 API、依赖或 automation 操作，不修改原生格式、身份、fingerprint、事务与插件生命周期。保留上一项未提交改动；本项不归档、不提交。
