# 验证记录

2026-09-19，开发基线 `7c726c7`。本变更未执行 git commit 或 push。

## 构建与回归

| 检查 | 结果 |
| --- | --- |
| Debug 全量构建及最终 Editor 增量构建 | 通过 |
| Release `hyperion_editor`、`scene_query_tests`、`spatial_tests`、`scene_instance_tests` | 通过 |
| Debug 定向 CTest | 11/11 通过，114.27 秒 |
| Release 定向 CTest | 3/3 通过，4.90 秒 |
| Release `--exercise-picking` | 通过，GPU validation errors = 0 |
| 全仓库文件名、include 大小写、格式检查 | 通过 |
| 模块边界检查 | 通过 |
| `git diff --check` | 通过 |
| OpenSpec change strict validation | 通过 |

Debug 覆盖 `scene_management`、`scene_spatial_visibility`、`transform_gizmo`、`scene_ray_queries`、`scene_boundary_contracts`、`scene_runtime_instance`、`scene_dispatch_failure`、`plugin_runtime`、`plugin_applications`、`depth_conventions` 和 `editor_acceptance`。Release CTest 覆盖 `scene_ray_queries`、`scene_spatial_visibility` 和 `scene_runtime_instance`。

Editor 验收走真实 GUI 输入路径，覆盖最近模型选择、空白清除与 Outliner 持久空选、拖动/右键导航/失焦/视图变化取消、预览相机及无效预览、resize、gizmo 优先、场景重开；并确认选择不改变文档脏状态或历史。原有文档、浏览相机、gizmo、加载失败和关闭中加载回归同时通过。

几何测试覆盖 bounds 命中但三角形未命中的继续遍历、父变换和启用状态、section/source 过滤、非均匀/镜像/剪切/奇异变换、近远裁剪、材质与 section 覆盖的面剔除、未调度 pass、材质版本替换、Render acknowledgment 后的独立失效、槽位复用、部分加载和 Worker 取消。场景实例测试验证默认不准备查询数据、开启后共享数据以及 Close 等待。

## 加速工作量

- 单个 4096 三角形网格：150 条固定种子射线与穷举结果一致，每条射线最多进行 8 次精确三角形测试；BVH 节点与索引数组的容量为 106540 字节，不包含原始顶点数据。
- 1024 个共享几何的模型对象：定向射线访问 11 个对象树节点、1 个候选对象、1 个三角形。
- 无编辑重复查询不重建/重拟合；变换产生 refit；元数据和材质更改不重建三角形数据。

这些是确定性工作量检查，不是任意场景的耗时保证。

## 已有验证环境问题

对 24 个改动 C++ 单元执行语义命名检查，唯一报告为未修改的 `Source/Runtime/Gui/Public/Hyperion/Gui/Gui.h:120` 中 `bool* bOutExpanded`。已核对 HEAD 中同一声明；当前 `CheckStyle.py` 的布尔类型查询未覆盖 bool 指针，误将该声明归入非布尔变量。未扩大本变更去修改 GUI 接口或检查器。

Debug 增量链接期间出现 MSVC LNK1163 COMDAT 错误；清除失败测试目标的生成 `.ilk` 缓存并重建后，全量构建和相关测试通过。没有修改编译或链接配置。

## 功能边界

以当前 Main 状态的静态几何为准，不模拟 alpha discard、透明混合或 shader 顶点变形。Hit 可以通过 `bIncomplete` 表示仍有未准备候选；没有命中且存在未准备候选时返回 Unavailable，Editor 保留选择。显式传入的模型数据由调用方准备 QueryGeometry，Scene 不在 Main 隐式构建三角形树。模型内部实例先逐个检测 bounds；内部实例数量很大时可在后续基于性能数据增加局部实例树。

## Outliner 灯光选择后点击 Sponza 的回归修复

新增验收先通过真实 Outliner 行选择 `light-courtyard-3`，确认远离 gizmo 的前景 Sponza 射线可以命中，然后在同一位置左键按住五帧并释放。修复前稳定失败：按住期间 `pending=0 focused=0 gizmo=0`，释放后仍选中灯光。此前相邻两帧按下/松开的用例未覆盖中间持续按住的帧。

原因是 GUI 图像没有交互 ID，ImGui 在按下帧结束时启动窗口背景拖动；下一帧图像因其他 active item 变得不可交互，Editor 取消了待完成的拾取。修复在 GUI adapter 中为图像注册交互区域，并与现有 overlay capture 共享身份，持续持有左键到释放。Scene 查询和 gizmo 优先策略无需修改。

新增覆盖普通图像/overlay capture 的八帧按住与小幅移动，以及真实 Sponza 的 Outliner 选择切换、gizmo 变换更新和文档历史不变。

- Debug 全量构建及最终 Editor 构建通过。
- Debug `transform_gizmo`、`scene_ray_queries`、`gui_docking`、完整 `editor_acceptance`：4/4 通过，125.02 秒。
- Debug `gui_input_and_data`、`gui_texture_rendering`：2/2 通过。
- Release Editor 与 GUI docking 测试构建通过；`gui_docking` 通过；最终 `--exercise-picking` 通过，GPU validation errors = 0。
- 格式、模块边界、`git diff --check` 和 OpenSpec strict validation 通过。
- 本次四个改动 C++ 单元的命名检查仍报告已有 `bool* bOutExpanded` 类型误判：`Gui.h:120` 声明及 `GuiWidgets.cpp:25` 定义；均已核对 HEAD，无新增命名问题。

本次修复继续保留在同一 OpenSpec change，未归档，未提交。

## 独立质量审计后的验证

独立审核确认并关闭两项 P2：float 命中距离向下舍入导致等距候选提前被剔除，以及拾取策略遗漏实际渲染视图的 legacy Forward pass 排除条件。复现证据、修复快照和独立复审结论见 `audit.md`。

- Debug 全量构建通过；相关日志为 `out/build/debug/PickingAuditFullBuild.log`。
- Debug 定向 CTest 7/7 通过，共 161.90 秒：`scene_ray_queries`、`scene_spatial_visibility`、`scene_runtime_instance`、`deferred_rendering`、`depth_conventions`、`transform_gizmo`、完整 `editor_acceptance`。
- Release `hyperion_editor` 和 `scene_query_tests` 构建通过；日志为 `out/build/release/PickingAuditEditorBuild.log`、`PickingAuditQueryBuild.log`。
- Release `scene_query_tests` 通过，包含等距候选、原始 near/far 约束和 8 种材质组合的 Deferred/Forward 查询策略回归；既有加速工作量断言保持通过。
- Release 真实 Sponza `--exercise-picking` 通过，报告 `out/editor-tests/picking-audit-release.json` 中 `picking_verified=true`、`document_dirty=false`、`validation_errors=0`。
- 全仓库文件名与格式检查通过（641 个源文件），模块边界检查通过（608 个源文件、35 个模块）。本次修复的 `SceneQuery.cpp`、`SceneRaycast.cpp`、`ScenePipelineViews.cpp` 和 `SceneQueryTests.cpp` 四个编译单元语义命名检查通过；前述 GUI bool 指针检查器问题仍单独保留记录。
- 原独立审核者复审关闭两项 finding，独立运行最新 Debug `scene_query_tests` 与 `git diff --check` 通过，未发现直接回归。完整构建和 GPU/Editor 验收由主 agent 执行。
- OpenSpec strict validation 与 `git diff --check` 通过；没有执行 git commit、push 或 OpenSpec 归档。
