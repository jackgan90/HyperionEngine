## Context

`ImportValidation.cpp` 的 Workspace 预检与 `AssetPublication.cpp` 的独立发布入口重复输出 `.hasset`、源/输出隔离及 sourceRoot/sourceId 规则；`ImportPreparation.cpp` 另有相同默认 library 推导。路径规范化本身已有 `IFileSystem::Normalize` 和 `ImportPath` 权威实现。

现有入口并非完全相同：Workspace 先校验 Main、generation、Game 根、只读、字符串长度/NUL 及挂载边界；分组导入拒绝任何显式 library。ImportAsync 允许分组 library 等于输出父目录，输出校验同步执行，而 source identity 校验在工作线程获取输出 lease 后执行。PrepareAsync 只读准备未执行这些完整发布准入检查。RootId 已共用 `IsAssetIdentifier`，不重复提取。

## Goals / Non-Goals

**Goals:** 每条重复基础规则只有一份实现；各入口独立调用、保持现有错误和副作用边界；回归实际公开调用而非仅测试 helper。

**Non-Goals:** 不改变可接受名称的集合（包括现有对任意 `..` 子串的拒绝），不强化路径安全策略、不重构发布事务、不扩展 PrepareAsync 准入、不新增公共/反射/automation API，不调整 plugin 装配。

## Decisions

1. 新增 AssetImport 私有 `ImportRules.h/.cpp`。`NormalizeImportLibrary` 接收已规范化输出并复用文件系统；输出本身继续调用现有 `Normalize`。`IsImportAssetOutput` 与 `IsSeparateImportOutput` 统一扩展名及隔离规则。避免新增重复的路径规范化层或暴露通用公共工具。
2. `CheckImportSourceIdentity` 返回 `EImportSourceIdentityError`（None/Incomplete/NonPortable），输入明确表达是否有 sourceRoot 及 sourceId。Workspace 保留合并错误，发布保留两种错误及先配对后逻辑名称的优先级。判定不依赖显示文本，不用布尔开关选择入口策略。
3. 保持每个调用点的检查顺序与错误阶段。PrepareAsync 仅共用默认 library；Workspace 的 `/Game`、只读、generation、4096/NUL、类型/scene/RootId 检查留在领域入口。发布仍通过自身准入，不依赖 Workspace。
4. 新的公开入口回归先在原实现运行，再用于重构后比较。覆盖来源标识组合、输出错误、默认及显式库、分组差异、异步错误及失败零写入；已有发布/CLI/GUI/草稿回归保护身份、freshness、事务和集成。

## Risks / Trade-offs

- 合并 helper 可能意外移动校验或改变异常 → helper 无业务异常文本，调用点保持检查顺序；测试断言同步/异步边界及既有消息。
- 归一化后与原始 sourceRoot 的空值判断不同 → 各调用点传入原先判定对象的 presence，不改变 root 处理。
- 保留入口差异使规则不呈现为单一“大 validator” → 差异属于各入口的策略，共享基础规则足够消除当前重复。

## Migration Plan

无持久化、API 或用户配置迁移。编译替换私有实现即可；回退可只撤销本 change 的代码/文档。上一项材质执行模式改动不属于本 change；本轮不归档、不提交。

## Open Questions

无。
