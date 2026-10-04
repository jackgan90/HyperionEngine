## Context

基线 `61b96e81867b8a70f4ff9d3ccfc80709139a2bb9`。Reflection 的 WriteRecord、wire 解码和 inspection 默认草稿各自构造同一种 `{type, version, fields}`。跨模块消费者反复执行嵌套 variant/map 解包。NativeAsset 还依赖宽松的结构识别规则遍历未知 Record 的字段，以生成既有依赖路径。

## Goals / Non-Goals

**Goals:** Reflection 拥有封装构造、访问与结构识别；消费者只表达要访问的 Record 语义；维持原有格式、引用所有权、迁移和失败顺序。

**Non-Goals:** 不增加反射能力或 descriptor 字段；不调整数据版本、字段 ID、wire schema、持久化格式、操作 ID 或业务策略；不统一独立的 JSON serialization 文档、JSON Schema、资产容器 header/object 或 Scene component type/value 格式；不重做 inspection path。业务 `name` 不是封装字段。

## Decisions

1. 增加 Reflection `RecordEnvelope.h/.cpp`，由 Record.h 暴露：`MakeRecordEnvelope`、const/mutable `RecordFields`、`RecordTypeId`、`RecordVersion`、`HasRecordEnvelopeShape`。工厂只包装数据，版本保持 uint64 archive node 编码，不执行 descriptor/domain 校验；允许 inspection 创建尚未满足业务约束的默认草稿。访问器返回原节点内的引用或版本值，不复制字段或 bulk，不创建缺失键。
2. 严格访问器只读取指定部分，保留 `std::get` / `at` 和既有 uint32 数值转换语义。ReadRecord 仍先读取 version，再检查 type/version 范围，随后读取 fields、迁移副本、绑定和校验；不提前验证整个 envelope。拒绝全部改成一个急切验证的 view，因为这会改变 malformed 输入的错误优先级。
3. `HasRecordEnvelopeShape` 仅要求 object、string type、object fields 和 version 键存在，不验证版本值、注册类型或业务字段。NativeAsset 复用此规则，合法结构使用点分字段路径，其他 object 保持括号路径递归；已识别的 AssetRef 仍调用完整反射校验。结构识别不是有效性保证。
4. `RecordFieldsKey` 由 Reflection 单点定义，供现有 archive-node inspection 路径拼接和 GUI ID 过滤使用。其值保持 `fields`，保留当前路径和观察器 ID；不扩展成新路径类型。其他封装键仅由实现维护。
5. 迁移生产消费者和插件内的相关测试，移除纯解包包装。保留仅在测试中构造旧版本/损坏格式的原始字面量作为独立兼容性 oracle。资产名称读取、缺省、清洗和插入/查找策略留在业务域；不创建 RecordName 或修改其他 roadmap 项。

## Risks / Trade-offs

- [默认草稿误触业务校验] → 工厂没有 descriptor 参数；用无效默认值的反射类型验证 inspection 行为。
- [错误顺序或依赖路径变化] → 分离严格访问与宽松结构识别，覆盖缺失/错型/无效版本、未知嵌套 Record、普通 map 的基线行为。
- [格式兼容性假阳性] → 测试用原始键构建独立预期 envelope，比较编码字节/hash；复用现有 wire、迁移、资产和编辑回归。
- [引用生命周期] → 访问器引用仍受输入节点生命周期及 map/variant 修改约束，公开接口说明；不缓存内部引用。
- [跨模块改动漏编译] → 构建相关测试及 Editor、AssetTool、Automation CLI，执行模块边界和命名/格式检查。

## Migration Plan

先建立 Reflection 接口，再替换消费者，增加针对性回归并更新 NativeAssets 文档。无磁盘迁移。独立 quality-audit 后主 agent 核实 findings、最小修复并针对性复审；用户验收 diff 前不归档或提交本阶段。回退仅需回退本 change 的代码。

## Open Questions

无。
