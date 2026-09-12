# 共享材质与纹理资产质量审计

日期：2026-09-12。审查范围是 `add-shared-material-texture-assets` 的全部未提交变更及直接调用链。基线为 `b49347c2090cad0ae085c918a8b85a95cc5179de`。未暂存、未提交 Git；未扩大到无关重构。

## 审查版本与方式

首次 reviewer 使用不继承主任务上下文的独立会话，只读审核 116 个变更文件，覆盖已跟踪和新增文件。主 agent 复核每项问题的调用链和复现证据后实施修复，原 reviewer 完成两轮代码复审，并复核最终测试时序调整。审核读取期间相关源码保持冻结。

- 初始清单：`out/SharedAssetAudit/Snapshot.json`，SHA256 `0ae50c5bc8f1a04dd072b355ef206ab6162595e0d246df6f40f9b6f9d4760acf`。
- 第一次修复清单：`out/SharedAssetAudit/FixedSnapshot.json`，SHA256 `5af792c8dfd00dbe44339a7571898c4de75b2e22a621874a5d2bc4c9b1e50ba4`。
- 运行时修复清单：`out/SharedAssetAudit/FinalSnapshot.json`，SHA256 `5b96ce69af275aedd512f42ba34601ab836ea3fd572c63ab465f34548b489b4c`，116/116 文件哈希匹配。
- 最终测试调整清单：`out/SharedAssetAudit/TestTimingSnapshot.json`，SHA256 `1dc968f6784c90676f6ac499811b8fa56e798fbe7791e8378b64cb87e2b16d55`，117 个文件。相对上一版仅增加两项测试的异步就绪等待/预热预算调整，运行时源码不变；之后补充本报告与验证文档。

重点覆盖独立资产反射、glTF/旧内嵌模型拆分、共享库与固定 revision、原生依赖加载、通用材质与纹理缓存、异步编译及 GPU 生命周期、场景加载期间编辑和保存、Viewer/AssetTool 集成与原有行为回归。

## 已确认问题与处置

| ID | 优先级 | 原触发条件与影响 | 修复与最终处置 |
| --- | --- | --- | --- |
| SA-01 | P1 | 场景材质尚未发布或加载失败时执行 Snapshot，源文件中的材质引用和覆盖值被空运行时状态覆盖 | 跟踪待发布的持久化选择；Snapshot 明确拒绝无法完整表达的待定/失败状态。删除实例或显式替换其数据时清理关联记录。已修复并通过复审 |
| SA-02 | P2 | 加载期间接受的整模型或分段材质编辑，被稍后到达的初始加载结果覆盖 | 在成功 Update 后记录已编辑字段和分段；发布只填充未编辑部分，保留编辑后清空的操作。已修复并通过复审 |
| SA-03 | P2 | 使用不传 Resources 的公开 LoadNativeModel API，更新未固定的材质依赖后，新模型仍显示几何聚合缓存中的旧材质 | 每个 FModel 从当前依赖数据取得声明快照；保持几何身份不变，并沿用 Worker 异步 shader 编译。默认与预准备两条 API 均覆盖旧/新版本共存。已修复并通过复审 |
| SA-03-R1 | P2 | SA-03 首轮修复中，声明快照仍存活但临时预编译快照释放后，缓存丢失原 Definition 身份并重复准备 | 同时保留声明与当前快照的弱引用候选，仅在两者均失效时清理；重建预准备包装时复用存活编译程序。保留容量限制，无额外强引用环。已修复并通过第二轮复审 |
| SA-04 | P2 | 同一场景引用同一材质/纹理 ID 的不同固定 revision，导入被错误判定为共享产品冲突 | 已通过原生头部与引用校验的不同代际可以共同发布；互相矛盾的来源命名产品仍拒绝。已修复并通过复审 |

主 agent 对 SA-01/02 执行 gated-IO 场景复现，确认引用/覆盖丢失和 0.62 被恢复为来源 0.15；对 SA-03 改用默认公开 API，确认加载对象已更新而新蓝色像素断言失败；对 SA-04 复核 reviewer 的独立双版本原生图夹具，并在修复后实际导入成功。SA-03-R1 由 reviewer 独立 CPU 探针确认，主 agent 复核缓存弱引用替换与清理路径后修复。

## 回归覆盖与证据

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| 场景待定/失败保存、加载中编辑与清空、变换和删除 | 通过 | `out/SharedAssetAudit/SceneRegression.log` |
| 默认/预准备 API 的依赖版本更新、临时快照释放、GPU 共享与保存回归 | 最终 Debug/Release 均通过 | `out/SharedAssetAudit/SharedGpuDeliveryEvidence.log` |
| 同 ID 不同固定 revision 共存、来源产品冲突仍拒绝 | 2/2 通过，含模型夹具 | `out/SharedAssetAudit/PublicationRegression.log` |
| 原始混合材质代际导入复现 | 修复后导入成功 | `out/SharedAssetAudit/RevisionMixMaterialsFixed.log` |
| 声明到预准备再释放的 Definition 复用 | 通过，MaterialPreparations=1 | `out/SharedAssetAudit/CacheUpgradeProbeFixed.log` |
| 声明/预准备交叉释放、编译程序独立存活与包装重建 | 通过，Definition 与 TextureSource 各 1 个 | `out/SharedAssetAudit/CacheReleaseOrdersProbe.log` |
| Debug 完整构建及最终测试增量 | 通过 | `out/SharedAssetAudit/BuildDebugFinal.log`、`BuildSceneDebug.log` |
| Release 完整构建及最终测试增量 | 通过 | `out/SharedAssetAudit/BuildReleaseFinal.log`、`BuildSceneRelease.log` |
| Debug 最终完整 CTest | 62/62，通过，328.70 秒 | `out/SharedAssetAudit/TestsDebugDelivery.log` |
| Release 最终完整 CTest | 62/62，通过，238.40 秒 | `out/SharedAssetAudit/TestsReleaseDelivery.log` |
| 格式/文件名/include | 430 个源码文件，通过 | `out/SharedAssetAudit/StyleDelivery.log` |
| 语义命名与局部声明 | 全量 265 个翻译单元通过；缓存修复涉及的 2 个、测试调整涉及的 1 个翻译单元再次通过 | `out/SharedAssetAudit/Naming.log`、`NamingFinalDelta.log`、`NamingSceneDelta.log` |
| 直接和传递模块边界 | 410 个 Source 文件、28 个模块，通过 | `out/SharedAssetAudit/BoundariesFinal.log` |
| OpenSpec 严格校验 | 通过 | `out/SharedAssetAudit/OpenSpecDelivery.log` |
| git diff --check | 通过 | 最终工作区检查 |

新增持久回归位于 `SceneInstanceTests.cpp` 的加载阶段编辑检查、`SharedAssetPublicationTests.cpp` 的 `CheckPinnedGenerations`/`CheckSourceProductConflict`，以及 `SharedAssetGpuTests.cpp` 的默认/预准备依赖检查和 `CheckRawCacheUpgrade`。缓存 GPU 回归通过公开 Resources/Scene 路径，验证临时 Worker 预准备释放后另一个同数据模型仍复用 Definition/Overrides，CPU 材质准备和纹理源计数不增长。

本轮第一次全量 Debug 测试与命名扫描并行时，`scene_viewer_acceptance` 在固定 180 帧内仅 1/514 个模型就绪，结果 61/62；停止其他负载后单独运行通过，证据为 `TestsDebug.log` 与 `SceneViewerRerun.log`。随后在无并行构建/命名负载的完整回归中，Debug 60/62、Release 61/62：两套 `scene_rendering` 都在切回默认材质后立即捕获到黑色；Debug 的场景局部失败夹具在 180 帧结束时仅 1/515 就绪、1 个预期失败，Release 该项通过。原始日志保留于 `TestsDebugFinal.log`、`TestsReleaseFinal.log`，未用重跑覆盖。

主 agent 与 reviewer 核对实际调用链：几何资源在发起材质准备后即可 Ready，初始选中的共享材质 Ready 不代表尚未使用的默认材质也 Ready；测试的 Frame 只等待当次 Render/RHI，不等待 Worker 准备。故 `SceneRenderTests.cpp` 在切换后调用既有 `AwaitBridge`，随后仍检查蓝色像素及取消订阅行为。Viewer 没有资源就绪后计帧的选项，因此 `SceneAcceptance.py` 将有限预算从 180 改为 900 帧，与已有原生图验收的预算一致；就绪数量、失败隔离、逐字节图像比较和缓存断言全部保留。900 帧仍不是逻辑就绪保证，必须通过实际断言；未为这两项失败修改运行时行为。最终完整 Debug/Release 按顺序运行，且未与构建或命名扫描并行；两套均 62/62 通过，分别耗时 328.70 秒和 238.40 秒，均包含 27 项 GPU/desktop 测试。

原始独立报告和两轮复审分别为 `out/SharedAssetAudit/ReviewerReport.md`、`ReviewerRecheck1.md`、`ReviewerFinalRecheck.md`；两项测试调整另见 `ReviewerTestTimingRecheck.md`。Reviewer 最终关闭全部已确认问题，未提出新的已确认缺陷。独立 reviewer 的检查和主 agent 的完整构建/GPU 回归分别记录，不将其混为同一组验证。

## 验证边界

没有未关闭的已确认审计 finding。结论限于所列版本、调用链和回归夹具；本轮未新增长时间内存压力、设备丢失或所有发布失败点的穷举验证，也不据此宣称大型场景性能提升。原任务的功能与非目标边界仍见 [验证记录](verification.md) 和 [共享材质资产文档](../../../../docs/SharedMaterialAssets.md)。
