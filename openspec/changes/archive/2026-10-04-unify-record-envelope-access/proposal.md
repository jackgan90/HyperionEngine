## Why

Record 归档的 `type / version / fields` 封装由 Reflection 产生，但资产、导入、GUI 和迁移代码反复手写解包规则。把格式访问收敛到拥有者可以减少维护重复，同时保留现有持久化和校验契约。

## What Changes

- Reflection 提供统一的 Record envelope 构造、字段引用、类型/版本读取和结构识别接口。
- 迁移生产消费者及 inspection 路径中的封装键；删除只重复解包的局部包装。
- 保持字节、hash、wire schema、迁移、错误上下文及资产依赖路径不变；补充针对这些边界的回归覆盖。
- `name` 继续由资产业务解释，不增加通用 RecordName，也不改变类型 ID、字段策略或 Inspector 路径模型。

## Capabilities

### New Capabilities

- `record-envelope-access`: Reflection 拥有 Record envelope 的格式和访问语义，消费者共享该契约。

### Modified Capabilities

无。现有反射、归档、原生资产和编辑能力的外部要求保持不变。

## Impact

涉及 Runtime/Reflection、Assets、AssetEditing、AssetImport、Scene、Gui，Editor/Automation 插件中的相关消费者和 AssetTool 的已有迁移实现。新增接口归属现有 Reflection target，不引入模块依赖、插件生命周期或用户可操作能力变化。原始格式测试可保留字面量作为独立兼容性证据。
