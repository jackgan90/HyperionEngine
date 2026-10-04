## Context

ObjectPlacement::Search 将空串或 All 解释为全部，AddCategory 禁止 All，Editor 又把该文本存为状态。目录与 automation 仅需要全部条目，不需要展示标签。

## Goals / Non-Goals

**Goals:** 显式可选分类过滤；保持开放字符串分类、顺序和搜索规则。
**Non-Goals:** 不改对象工厂、placement lifecycle、场景操作或持久化。

## Decisions

1. Search 接受 optional<string_view>：nullopt 表示不过滤，有值仅精确匹配类别。Editor 状态使用 optional<string> 持有身份，查询临时借用视图；不用 bool 加冗余字符串，避免矛盾状态。
2. All 只存在于 palette 的显示按钮，按钮使用独立稳定 GUI ID；真实类别也有自己的控件 ID。移除注册的 All 保留字，注册空类别和重复类别仍拒绝。
3. 目录、报告、资源准备明确传 nullopt。未知/空的已指定类别不匹配任何对象，关键词保留 case-insensitive 标签/ID 搜索，多类别对象仍只返回一次。

## Risks / Trade-offs

- [旧 C++ 调用方把 All 当通配符] → 全仓迁移并搜索确认；无外部 wire 分类过滤字段需要迁移。
- [同名 GUI 控件冲突] → 不过虑按钮与类别按钮使用不同 ID。
- [string_view 生命周期] → 仅同步查询借用，不在 registry 保存过滤视图。

## Migration Plan

迁移 API 和全部调用，验证真实 All 类别与未知/空过滤，再跑 placement/目录回归。无资产迁移；验收后再归档提交。

## Open Questions

无。
