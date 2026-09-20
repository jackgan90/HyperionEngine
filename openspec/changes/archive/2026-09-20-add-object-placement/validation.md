# 放置对象验收记录

日期：2026-09-19。变更：`add-object-placement`。保留工作区改动与活动 OpenSpec 变更，不提交、不归档。

## 实现与验证范围

- Window > Place Object 可重新打开面板；All / Basic / Shapes / Lights 使用同一类型注册表，支持搜索和多分类。
- 五种原生基本形状和 DirectionalLight / PointLight / SpotLight 支持跨面板拖拽、持续世界空间预览、一次创建事务、Undo / Redo。
- 预览与场景节点、查询、光照、保存和历史隔离；落点支持表面法线和形状边界，以及地面 / 对焦平面回退。
- 八条取消路径覆盖 Esc、失焦、视口外释放、模态窗口、隐藏视口、窗口尺寸、应用缩放和文档世代变化，保留原有重做分支。
- 光源图标支持真实鼠标点击选择、隐藏及禁用过滤；主方向光的首次选择、追加不替换、显式切换与撤销重做均已验收。
- 从空文档放置、Save As、重载恢复模型引用与光源。缺少 Cube 或 PointLight 资源时只禁用对应条目，场景仍可用。
- 原有三角形拾取验收的合成测试场景关闭光源图标，避免位于原点的 `light-main` 按新规则优先接收点击；正常场景与专门的图标点击验收仍启用图标。
- Release 的原有数值拖拽验收曾失败，诊断为 `history=0/1 text=1`：紧接文本编辑的再次点击进入了文本输入。验收在新手势前清除修饰键，并保持帧循环运行、等待 0.5 秒以避开双击窗口；不改变控件或历史实现，也不放宽事务断言。修正后的 Release 编辑器完整验收已通过。

## 构建与测试

```powershell
./tools/Build.ps1 -Preset debug
./tools/Build.ps1 -Preset release
ctest --test-dir out/build/debug --output-on-failure -R '^(scene_ray_queries|scene_runtime_instance|plugin_runtime|plugin_applications|render_graph|deferred_rendering|gui_input_and_data|object_placement|gui_docking|gui_texture_rendering|editor_placement|gui_scale_acceptance|editor_acceptance)$'
# Release 使用相同表达式，将构建目录替换为 out/build/release。
```

| 检查 | 结果 |
| --- | --- |
| Debug 全量构建 | 通过，编译 / 链接日志无警告或错误 |
| Debug 相关回归 | 13/13 通过，147.27 秒；最终验收手势调整后 editor_acceptance 再次通过（97.44 秒） |
| Release 全量构建 | 通过，编译 / 链接日志无警告或错误 |
| Release 相关回归 | 12 项首次通过；修正测试手势后 editor_acceptance 通过（19.54 秒），13 项均完成 |
| CheckStyle.py | 文件名、include 大小写和格式通过，660 个源文件 |
| 语义 C++ 命名和局部声明 | 36 个修改编译单元通过（35 个批量检查，1 个最后修改的验收单元单独检查） |
| CheckBoundaries.py | 626 个源文件、35 个模块通过 |
| OpenSpec strict | 通过 |
| git diff --check | 通过 |

语义检查沿用 `CheckStyle.check_naming`，输入为 Git 已修改 / 新增的 C++ 编译单元，使用 Debug 编译数据库。检查器同时修正了已有 `bool* bOutExpanded` 被非布尔前缀检查误报的问题，只允许指向 bool 的指针使用该前缀。

相关日志保存在 `out/PlacementDebugBuild.log`、`out/PlacementDebugTests.log`、`out/PlacementNaming.log`、`out/PlacementReleaseBuild.log` 和 `out/PlacementReleaseTests.log`。验收手势调整的增量构建、命名检查与复验另保留为 `out/PlacementDebugAcceptanceBuild.log`、`out/PlacementReleaseAcceptanceBuild.log`、`out/PlacementAcceptanceNaming.log`、`out/PlacementDebugEditorRetest.log` 和 `out/PlacementReleaseEditorRetest.log`。

## 资源与视觉检查

`hyperion_asset_tool --mounts ContentMounts.json --authoring build-placement` 已成功运行；对全部 10 个原生资源在重复生成前后比较 SHA-256，结果一致。生成的透明 PNG 原图、提示词及导入配方位于 `Content/Editor`。

已检查真实编辑器的网格拖拽预览、方向光预览和最终场景截图。Debug 放置验收保存重载后报告 9 个对象、5 个模型绘制项、0 个失败模型、0 个 GPU 验证错误；结束时的 graphics 验证同样为 0。截图位于 `out/editor-tests/placement-*/`，最终预览副本为 `out/editor-placement/Final.png`。

## 当前约束

保持单视口、Deferred / reversed-Z 的现有编辑器范围。预览不自动旋转、不吸附网格；CPU 表面查询沿用静态三角形限制。图标在场景几何前方叠加，当前仅主方向光参与方向照明。无光照的空场景中，提交后的正式网格可能为黑色；拖拽预览使用独立着色。操作说明见 `docs/Editor.md`，动态资产注册契约见 `docs/SceneManagement.md`。

## 2026-09-20 独立质量审计与修复

以 `6d1359709d56b27d077d9b4d0cb31231fc2199bd` 为基线，冻结本次放置功能的 85 个未提交文件；两位不继承实现会话的 reviewer 分别审查 Editor / Gui 与 Scene / Renderer / 原生资源。主 agent 复核后修复两项 P2，原 reviewer 已针对修复复审，86 个文件的复审快照哈希全部一致。审计仍未提交、未归档。

- **EDI-01，已修复：** 重叠光源图标原来按节点顺序绘制、按深度拾取，可能选中被盖住的灯。现在绘制与拾取使用相同投影候选排序，远到近绘制，反向命中，平深度保持原场景首项优先。新增真实 GUI 验收检查近点光 / 远聚光、反向深度、相同深度三种场景的绘制纹理顺序及鼠标点击结果。
- **SP-01，已修复：** 去重时旧 ID-only 失效引用的解析异常会阻断有效模型注册。现在只跳过该旧候选，保留原错误，新请求仍独立验证。回归覆盖有效注册、去重、新失效请求拒绝、旧错误保留、节点就绪、射线查询和仅保存 live 资源。
- **SP-Q01，待确认：** 非空 `GetDrawResults()` 也可能包含默认、未就绪结果，因此不能单独证明正式主视图已经绘制。当前 Editor 提交当帧会 Tick / Render，且资源提前准备；尚未复现真实内置对象交接中的可见空帧，未据此改动生产代码。后续需在保持真实帧序的 harness 中延迟正式材质、保留预览资源就绪，记录连续帧的预览包与主视图结果，并覆盖失败终态。未复现不等于排除。

本轮验证结果：

| 检查 | 结果 |
| --- | --- |
| 修复前回归 | 两个新增检查分别复现旧引用异常和错误图标覆盖顺序 |
| Debug / Release 全量构建 | 均通过，编译 / 链接日志未发现警告或错误 |
| Debug 直接相关回归 | `scene_runtime_instance`、`object_placement`、`editor_placement` 均通过；资源测试初轮等待条件读取了上一 Tick 的缓存状态，补足创建后的 Tick 后单项复测通过 |
| Release 直接相关回归 | 同上 3/3 通过，9.69 秒 |
| 原有完整 `editor_acceptance` | 本轮受内容环境阻塞：Sponza 加载后超时；未修改的旧 Release 程序也明确报告 Sponza Git LFS 实体缺失，不计作通过 |
| 格式 / 边界 / 命名 | 661 源文件格式通过；627 源文件、35 模块边界通过；6 个修改编译单元语义命名通过 |
| 独立复审 | EDI-01、SP-01 均确认修复；SP-Q01 保留待确认 |

初审时 `../HyperionAssets/Scenes/Sponza.hasset` 是 Git LFS 指针，场景实际内容未在工作区就绪；初审未修改内容仓库。上文 2026-09-19 的完整回归记录不替代当时被阻塞的验收。

完整审计、两个 reviewer 的初审 / 复审报告、固定哈希快照和原始日志位于 `out/audit-object-placement-20260920/`。关键证据为 `RegressionBeforeFix.log`、`DebugTests.log`、`DebugSceneRetest.log`、`ReleaseTests.log` 和 `BaselineSponza.json`。

### Sponza LFS 恢复与补验

2026-09-20 用户反馈 revert 后文件变小，进一步诊断确认：真实对象仍在本地 LFS 缓存，大小 39,204 字节，SHA-256 为 `2442f11ef09d2d2641bf7260987097165c46aeefd45e127b0d754b7ea69f9233`。内容仓库的本地 `filter.lfs.smudge` 与 `filter.lfs.process` 配置带有 `--skip`，导致工作区留下 130 字节指针。先前加载错误中的“LFS object is missing”不能据此推断缓存丢失。

已用定向 `git lfs checkout Scenes/Sponza.hasset` 从验证过的缓存恢复文件，并仅去掉内容仓库这两个本地过滤器中的 `--skip`。还原后的内容与提交指针哈希完全一致；`git cat-file --filters` 也验证自动展开后输出相同 39,204 字节，无须网络下载，未提交。

恢复后，完整 `editor_acceptance` 在 Debug 通过（130.78 秒）、Release 通过（24.61 秒），此前的 Sponza 验收阻塞解除。日志为 `DebugEditorAfterLfs.log`、`ReleaseEditorAfterLfs.log`，过滤器验证为 `LfsFilterVerification.json`。SP-Q01 交接预览疑点仍保持原来的待确认状态。
