# 多对象选择与编辑质量审计

## 审查范围

基线为 `1cf64dd84331890842f1f1c160416dd02039c1a8`，审查覆盖当前 change 的全部 67 个未提交文件，包括新增文件，没有排除项。独立 reviewer 未继承实现会话上下文，初审期间源文件保持冻结；主 agent 独立复核 findings 后统一修复，再由原 reviewer 针对修复和直接影响复审。

覆盖选择顺序与主对象、Ctrl 按下时的修饰键、Outliner/viewport 高亮、组件交集与混合值、逐字段绝对赋值、Scene/SceneInstance 批量原子性、gizmo 成组仿射变换、父子去重、取消与历史合并、多子树删除和恢复时的句柄重映射。Details 绝对赋值、仅 gizmo 成组变换的已确认边界保持。

## Findings 与处置

| ID | 优先级 | 确认的问题 | 修复与复审结果 |
|---|---|---|---|
| MS-01 | P2 | 混合数值为 3、7 时输入主对象现值 3，失焦后次对象仍为 7；只有 Enter 才广播。 | GUI 适配器识别合法同值文本的显式编辑，实时广播；保留 Enter 接受，无输入失焦不广播。主 agent 的标量探针和 reviewer 的独立向量轴探针均复现旧问题；原 reviewer 用相同探针源码重新链接验证修复，已关闭。 |
| MS-02 | P2 | 总体可见但所有实际 mesh sections 隐藏的非主选中模型，没有 outline、gizmo 或原点标记。 | 原点 fallback 根据实际模型 instances 及匹配 section 的可见性判断，保留 source-node/primitive 筛选。真实编辑器回归检查 GUI 绘制数据；主 agent 和 reviewer 分别查看截图确认菱形标记、主对象 outline/gizmo 与 Outliner 高亮，已关闭。 |

两项均经主 agent 独立复核确认，属于本次功能范围。修复只涉及六个实现/测试文件，没有修改公共协议或模块职责。复审未发现新的可确认问题，没有尚未解决的已确认 finding。

## 验证

- Debug 编辑器和受影响 GUI 测试重新编译、链接成功。
- 新增数值回归覆盖 double 标量、float 向量轴、合法同值文本、无输入、非法中间文本、Enter 和失焦，检查即时赋值及其他轴不变。
- 真实编辑器回归检查 Enter/失焦前已经同步、单条批量历史和精确 undo/redo，以及全部 sections 隐藏的非主对象标记。
- 五项 CTest 通过：`gui_input_and_data`、`gui_docking`、`editor_multiselect`、`editor_acceptance`、`editor_outlines`。编辑器 GPU validation 为 0。
- 格式、文件路径、模块边界、五个受影响翻译单元的语义命名、`git diff --check` 和 OpenSpec strict 校验通过。
- Reviewer 初审独立运行 Scene、gizmo、Reflection 和 GUI 测试；复审独立运行相同输入探针及新 GUI 测试，并核对编辑器原始日志和截图。

## 版本与证据

初审和复审记录、输入探针、构建/测试原始日志位于 `out/AuditMultiSelection/`：

- `InitialReview.md`、`MainVerification.md`、`ReReview.md`。
- 初审 `InitialSnapshot.json` SHA-256：`A078650945F5E4CC11E470EF727357F47C49A3F946AD1B1288A782B90D329991`。
- 修复后 `RepairSnapshot.json` SHA-256：`50AFB5F457D25966294958278D2EA00BB8B9F83E1C393EEA74A8B35E742C8A59`。
- Reviewer 和主 agent 均逐文件核对 67 文件快照无漂移。此审计记录及 validation 的收尾补充在复审完成后写入，不改变被审源码。

本轮没有 Release 验证或内存分配失败注入；reviewer 未另行重复完整 GPU 编辑器套件。复审限定于已确认问题及直接影响，不代表对所有潜在图形/资源边界情形的无条件保证。

审计结束时，所有改动保持未提交，OpenSpec change 未归档。用户随后于 2026-09-20 授权归档和 Git 提交；归档交付记录见 validation.md。
