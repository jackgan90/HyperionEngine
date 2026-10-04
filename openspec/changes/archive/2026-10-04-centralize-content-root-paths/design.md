## Context

FContentRootService 已拥有 Game 挂载状态。目录查询、导入验证和 standalone asset.open 分别使用同一根边界与未设根规则，但诊断文字及异常类型不同。Editor ContentRelativePath 与通用 FGuiPathDisplay::Value 重复。IO 已有通用包路径规范化、挂载和物理边界实现，不应向上依赖 Content。

## Goals / Non-Goals

**Goals:**

- Content 单次定义 Game/Engine 根身份和按 `/` 分隔符匹配的词法归属查询。
- 三个拒绝未设根的入口共享 Content 检查，保留各自错误分类和文本。
- Editor 复用根身份及现有展示转换；保留数据中的绝对包路径。
- 用边界、校验顺序、状态副作用及协议快照验证等价性。

**Non-Goals:**

- 不修改 IO 规范化、大小写规则、物理路径支持、符号链接、权限或遍历校验。
- 不把词法归属查询作为路径有效性或权限证明。
- 不新增根、用户功能、automation 操作、错误码目录或插件生命周期。
- 不清理说明文案、协议示例、固定测试 oracle 中的 `/Game`；隐藏目录过滤留到后续阶段。

## Decisions

1. 新增 ContentPaths 公共契约，定义 `GameContentRoot` / `EngineContentRoot`，提供 `IsGameContentPath` / `IsEngineContentPath`。查询匹配根自身或根后 `/`，区分 `/Game` 与 `/Gameplay`，大小写敏感，不规范化 `.`/`..`、重复斜杠或反斜杠。完整路径校验仍发生在原有调用位置，避免改变错误优先级及支持的物理路径。
2. `RequireGameContentRoot` 接收已获取的 `FContentRootInfo` 与调用方诊断文字，由 Content 统一检查目录为空并抛出 `FContentRootError("root_unset", ...)`。接收快照避免重读 Info 引入新的 Main/failed 检查。AssetImport 与 Automation 在局部 catch 中转换为原有异常类型，不捕获或转换此前 Info() 自身的失败。
3. 目录查询保持 generation、limit、包根/反斜杠、父级遍历、未设根的原始顺序。导入保持 generation、未设根、只读、字符串/输出路径顺序。Automation 保持 workspace 优先，Roots 存在时先 Info()，然后只对 Game 路径检查；Roots 为空的既有 CPU 使用方式不新增限制。
4. 移除 Editor 的 ContentRelativePath，两个 GUI 消费点直接用 `FGuiPathDisplay{GameContentRoot}.Value(...)`。通用 Gui 不认识 Content；路径展示依赖继续由 Editor 组合。目录默认值、显示配置、输出路径构造和 Game 索引筛选复用常量/查询，保留索引筛选原来仅匹配根下项的边界。
5. 新代码进入既有 Content target；独立编译 Browser 的测试显式声明已有使用的 Content 依赖。新回归接入既有测试 executable，避免新增独立 target 后漏接 aggregate 的构建依赖。

## Risks / Trade-offs

- [复用查询会无意规范化输入或接纳相似前缀] → 保留纯词法规则，测试大小写、根自身、相似前缀、斜杠和父级跳转。
- [统一检查改变错误类型、提示或前置顺序] → 保留局部异常适配，并覆盖多重无效输入、Info() 原始异常及未设根无副作用。
- [引入 Content 与 Gui/IO 的循环依赖] → Gui 展示复用由 Editor 调用；IO 完全不改；检查 direct dependencies。
- [字面量减少但契约变化未被发现] → 保存 7 个 operation describe 响应，独立错误 oracle 与现有实际 Editor/导入回归。

## Migration Plan

无数据迁移。实现、构建和针对性验证后，独立审核冻结快照；用户验收后再归档提交。
