## Context

FImportDraftHistory 是已注册的反射请求，action 当前是默认空值的任意字符串。DraftHistory 先调用 MutableDraft（Main、存在性、busy、generation），然后识别 reset/undo/redo。未知动作并不会在反射读取时失败；直接改为 enum 或严格字符串投影会改变 schema、默认值、未知字符串往返和错误优先级。

## Goals / Non-Goals

**Goals:**

- GUI、原生调用及历史算法使用领域枚举。
- 字符串只保留在既有协议请求及其所属领域边界，未知输入仍按旧优先级拒绝。
- 历史事务、generation、dirty、undo/redo 可用性和发布隔离保持等价。

**Non-Goals:**

- 不改变反射定义、默认字符串、operation ID、消息或 wire/archive 格式。
- 不引入通用 enum 字符串框架，也不修改其他状态、灯光诊断、截图目标或关闭流程。
- 不改变草稿历史容量、源转换、发布、插件组合或生命周期。

## Decisions

1. 在 AssetImport 的 ImportDraft.h 定义 EImportDraftHistoryAction（Invalid、Reset、Undo、Redo）。Invalid 只代表边界未识别的请求，在领域执行时拒绝。原生错误枚举值同样落入已有 invalid_arguments 分支。
2. 保留 FImportDraftHistory 及其 Action 字符串的反射契约，明确其协议请求职责。FAssetImportWorkspace 增加接收 FImportDraftMutation 与动作枚举的重载，成为唯一历史实现。旧请求重载在 AssetImport 内做大小写敏感解析后委托；解析不抛错，继续由类型化实现先运行 MutableDraft。GUI 和原生调用直接使用类型化入口。
3. 无需扩展 ImportStateReflection：生命周期字符串描述有限输出状态，但历史 action 属于开放的输入字符串，未知值原先必须晚于身份/状态/版本校验失败。保留协议 DTO 避免额外 raw-token 储存、双字段状态和人为改动反射。
4. 三个已知协议 token 的解析只定义一次。Automation 继续通过既有反射请求调用共享服务，不新增 transport、schema 或历史分支；合法操作 example 保留为协议文字。
5. 测试覆盖旧 schema/default/archive/wire（包括未知输入），直接比较枚举与协议入口的事务结果，验证未找到/busy/stale/非法动作优先级、undo/redo 边界、reset 可撤销及无发布写入；JSONL/MCP 的现有脚本补充错误与无副作用断言。

## Risks / Trade-offs

- [为了类型化提前拒绝未知输入] → 解析返回 Invalid；MutableDraft 仍先执行，旧 wire 请求反射完全不变。
- [协议与类型化入口各有历史实现] → 旧入口仅解析/委托，GUI 直接调用同一个类型化算法。
- [默认或未知字符串往返变化] → 固定旧格式样本和 schema 验证，不使用新实现生成测试预期值。
- [新增能力被误认为没有 automation 适配] → 本阶段只替换既有领域入口，保留已注册操作；文档说明协议请求与领域动作的边界。

## Migration Plan

无数据迁移。按同一阶段更新 GUI 与原生调用，构建与定向验证后冻结独立审核快照，用户验收后再归档提交。
