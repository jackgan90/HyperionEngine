## 1. Baseline

- [x] 1.1 核实两个入口与只读准备的规则、错误阶段和入口差异，完成设计与规范。
- [x] 1.2 添加公开入口行为回归并在原生产实现上运行，记录基线。

## 2. Shared implementation

- [x] 2.1 提取私有 ImportRules，接入 Workspace、发布及准备，保持检查顺序与错误语义。
- [x] 2.2 更新资产文档并人工核查共享事实、路径权威、入口策略、事务与文件/函数规模。

## 3. Verification

- [x] 3.1 完成 Debug/Release 相关构建及导入、发布、CLI、Editor/automation 和草稿回归。
- [x] 3.2 完成格式、命名、边界、diff 与 OpenSpec 严格校验，记录结果并确认不归档、不提交。
- [x] 3.3 按独立审计建议补充只读准备与发布准入差异回归，完成 Debug/Release 验证和原 reviewer 针对性复审。
