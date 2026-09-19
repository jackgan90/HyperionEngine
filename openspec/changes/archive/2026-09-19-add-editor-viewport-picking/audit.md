# 独立质量审计

2026-09-19，按仓库 `quality-audit` 技能执行。未提交、未归档。

## 审查版本与范围

- 基线：`7c726c7a22089956bacd714085ab78b2741c1adb`。
- 首次审核：全部 55 个未提交改动及新增文件，无 staged 文件和已知无关改动。排序后的文件路径与 SHA256 清单摘要为 `1e6ee19bc0d6f8bcfe73d561081780eacfc896fee5d708baf05d2c39b7db846e`。
- 修复复审：57 个文件，清单摘要 `7f988e5de4297c3f92782ee606fe5caf0643707cc4b3d9f93eaee5989047fe21`。该摘要不包含之后添加的本记录及任务勾选、验证记录更新。
- 审核者通过 `fork_turns: none` 创建，未参与实现；只读审核期间主 agent 冻结源码。原审核者完成针对性复审。
- 范围涵盖 Math 射线/BVH、Scene 查询与事务失效、Worker 准备及关闭、Renderer 视图策略、Editor/GUI 输入与选择、测试、文档及 OpenSpec。

## 已确认并关闭的问题

| ID | 优先级 | 触发条件与影响 | 主 agent 复核与处置 |
| --- | --- | --- | --- |
| R1 | P2 | `SceneRaycast.cpp` 和 `SceneQuery.cpp` 将向下舍入后的 float 命中距离直接作为遍历上限，提前剔除等距候选，绕过稳定 tie 比较。 | 独立运行最小复现确认。新增 `SceneRayMaximum`，以向上一个可表示值保留候选，并夹紧当前上限。对象、实例、三角形和奇异变换 fallback 均接入；保留原始 near/far。对象与跨三角形 BVH 子节点回归通过。 |
| R2 | P2 | `EditorPicking.cpp` 只配置 usage 名称，遗漏 legacy Forward 排除条件。只有 `HdrForwardOpaque + Forward` 的材质在 Deferred Editor 不绘制，却可被查询命中。 | 核对实际 `IsExcludedFromView` 与视图构造并独立运行复现确认。Scene 增加逐 usage 排除配置；Renderer `MakeSceneRayOptions` 与渲染视图共用 legacy 排除定义，Editor 使用该 CPU 配置。8 种材质组合的 Deferred/Forward 回归通过。 |

R1 原始输出：两个对象共用 `z=.1f` 的命中面，slot 1 有更靠近射线的非命中几何 bounds；应选 slot 0，实际 slot 1，候选数为 1。精确距离 `0.89999999850988388` 缩成 `0.89999997615814209`。`z=0` 对照选择 slot 0、候选数 2。

R2 原始输出：`material_default_deferred_passes=000 legacy_excluded=1 query_hit=1`。复现源和构建脚本保留在生成目录 `out/review-picking`；正式回归已纳入 `SceneQueryTests.cpp`。

## 用户重点与复审

Outliner 选择 light 3 后，多帧按住再释放点击 Sponza 的 GUI 输入路径、选择同步和 gizmo 变换更新已纳入编辑器验收。审核核对了图像交互 ID、gizmo 优先级、取消条件和不写文档历史的路径，未发现该修复的确认缺陷。

原审核者对修复快照独立运行最新 Debug `scene_query_tests` 和 `git diff --check`，确认 R1、R2 均可关闭，未发现直接回归。首次审核还独立运行了 `spatial_tests` 和 `gui_docking_tests`。主 agent 的构建、GUI/GPU 回归与检查结果见 `validation.md`。

## 边界

无未关闭的确认 finding。独立审核者未运行完整构建或 GPU/Editor 验收，这些由主 agent 执行。静态几何查询仍不模拟 alpha discard、shader 变形或逐像素透明效果；未将这类既定范围边界列为缺陷。
