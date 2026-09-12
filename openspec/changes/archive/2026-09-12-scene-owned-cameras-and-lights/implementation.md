# Implementation — scene-owned-cameras-and-lights

> This document preserves chronological implementation evidence. Stage-local pending statements describe that stage only. The final status and unresolved limits are in review-handoff.md and the final section below.

## Authorization and scope

2026-09-12: The user assigned implementation and testing of groups 1–12 to the main agent. Group 13 review is deferred; no Git staging, commit or archive is authorized by this delivery. Earlier planner/executor role restrictions are superseded by this instruction.

## Starting state and baseline

- HEAD: `ee585cc88fd60a22a29dc2103cd4dafe6d234221`.
- Executor residual changes were preserved under `out/SceneOwnedCamerasAndLights/MainRun/StoppedExecutor/` and `ResidualManifest.patch`, then reverted with explicit user authorization after the other executor stopped.
- Independent pristine tracked-source snapshot: `out/SceneOwnedCamerasAndLights/MainRun/Baseline/`. Source assets, shaders and configs are frozen there, with hashes in `BaselineInputs.json`. Locked third-party source is shared through its `out/deps` junction; its pinned revisions remain unchanged. Each baseline build generates its own binaries and native content.
- Debug/Release: Tracy OFF, RenderDoc ON, D3D12 validation preserved. Hardware: RTX 5080, driver 32.0.15.9186.
- Initial baseline Debug compilation failed with MSVC C1090 PDB service error 0x000006BA; raw log retained. Retry is in progress. No validation pass is inferred from the interrupted executor's logs.
- Consumer inventory: `MainRun/Consumers.txt`.

## Progress

Context and contracts read; implementation and verification evidence will be recorded below as completed. All unverified tasks remain unchecked.

## CPU 节点与初步迁移验证

完成第 2、3 组，独立数值 oracle、4096 层反序安装、事务失败状态保持、typed tombstone、settings 清理与默认内容测试通过。日志：MainRun/CpuVerifiedBuild.log、CpuTests.log、MigrationCpuBuild.log、MigrationCpuTests.log。格式与模块边界通过；native 迁移的旧 nested diagnostics 断言保留并通过。

基线素材与源码、独立二进制已冻结。Sponza 在初始 240 帧 warmup 未 ready，增至 10000 帧后 Debug/Release 均取得 840 样本且 validation 为零；初步采集不替代 V3 交错性能验证。完整 Debug 基线 CTest 61/62，scene_moving_camera readiness 失败，当时 Release 编译重叠，后续单独复测。

## v4/source v2 与场景帧集成中的验证

第 4.1–4.5 项完成。SceneSourceTests.cpp 包含 v1/v2/v3 native root 迁移、v4 roundtrip、ID 避碰、完整 affine matrix/材质/section 保留、plain v2 零模型与反序 parent、非法字段/选择/镜头/姿态等负例，publication_tests 通过（MainRun/SceneSourceBuild.log、SceneSourceTests.log）。主轴 X=0 但 forward/up 有效是 D3 明确允许的矩阵，作为正例；实际 basis 退化使用 Y=0 负例。

SceneInstance 已安装统一节点、typed 编辑、Handle/epoch 绑定材质完成结果、model-only 数量、独立 v4 Snapshot 与全记录引用 rebase。原先 pending material/slot generation/显式 Data replacement/Close tests 保留。新增无模型多相机场景保存/reload与 token/光照缓存测试通过。SceneFrameTests.log、ScenePipelineTests.log 均为真实 D3D12 Debug layer enabled 运行；这些不是完整 V1/GPU 像素验收。

Bridge 增加 metadata-only/empty-first serial=1、独立 scene receipts、epoch 与单次 primitive+metadata admission。scene seed/final frame 区分，Render 精确匹配 token；有效 Scene scope 与相机 motion 分离。Forward/Deferred 已接入共同解析与无相机 clear+extensions，CSM 结合 selected light 的投影与非零 radiance；viewport aspect 同步进入 CSM preparation key。当前仅定向编译/测试通过，Viewer 迁移、gated failure/resource/pixel tests、全配置检查与 V3 A/B 尚未完成。


## Viewer/GUI 迁移及门控验证（2026-09-12）

SceneViewer/ModelViewer 已通过 scene node 读取、编辑相机与灯光；应用冻结 scene seed 后立即 dispatch。场景树选择完整 Handle，支持 typed 创建/复制/编辑/两种删除/reparent 与三项 settings。源样例全部为 v2，原 manifest 消费者和集成脚本已迁移。完整 Debug 编译通过 ConsumerDebugBuildVerified.log；随后 scene_management、scene_runtime_instance、scene_viewer_controls、scene_rendering、model_rendering、shared_asset_publication、shared_asset_rendering 及其 fixtures 共 9/9 通过 ViewerNodeRegression.log。

新增外部相机编辑后 dolly/orbit/pan、切换相机、不可逆 parent 编辑原子拒绝测试。FScene::Update 的显式 Data 替换现在在同一次 CPU 事务内清除旧 asset association，避免第二次提交失败导致半更新。

F01/F02：确定性 Render gate 排入 P1/F1/P2/F2，geometry positions、camera eye、direct light values、两帧红/蓝实际像素、diagnostic tokens 均验证。F03/F05：RHI gate 下构建延迟准备的旧 graph，Main 删除全部节点并发布，释放后旧图保持红色，新图无相机 clear；旧 epoch reattach 拒绝。原 retained constants/resources/local cache assertions 均保留。第一次 RHI gate 试验未使用 deferred preparation，触发测试自身等待超时，已改用既有 deferred 路径；失败日志 GatedSceneTests.log 保留，通过日志 GatedSceneTestsRetry.log。

F04：由尚存 explicit primitive 触发首个 camera-only logical publication 的 Render 合约失败；确认 receipt 错误、scene-wide error、后续 frame 阻止、Close 清理与 reattach 恢复。此测试发现并修复首次发布未安装 metadata 时 cleanup serial=2 被错误拒绝的问题。PublicationFailureTests.log 全部通过。Main dispatch failure/retry 和其他 V1/V3 项仍需收口，不能将此结果当作完整 F04 覆盖。

M01：五个保留语义逐个 custom provider 负例；bound Scene/Global batch、Frame、View、pass 注入全部拒绝；检查冲突 batch 后原 custom Scene/Global 值保持，绑定前 View 内建值也拒绝。MaterialGuardTests.log 中 scene_instance_tests 全部通过。

初步 candidate 截图 CandidateCaptures/InitialResults.json：Debug Showcase-static max=2 mean=0.00007536；Model-static max=2 mean=0.00069316，均在原阈值 max<=2/mean<=0.1 内。Showcase-moving max=109 mean=0.00477717，最大值超阈值，保持失败记录，未放宽阈值；相机连续姿态计算与边缘像素差异待进一步定位。Release/Sponza candidate 和完整交错 A/B 尚未执行。

## Additional verified publication, cache and GPU cases

- `CheckSceneImporterCache` in PublicationTests passes (`ImporterCacheTests.log`): source-v2 conversion writes native schema 4/importer revision 2, repeat import is up-to-date, editing focus retains asset identity but changes revision, legacy native input upgrades. Existing optional model/material/texture dependency and pin tests remain covered by shared asset publication tests.
- `CheckContextMigrationContract` passes (`ContextMigrationTests.log`): registry identity compares migration contexts and rejected fields; overlapping migration registrations and current-version retired fields reject; old-version migration consumes the retired field.
- `CheckSceneViewSelection` and `CheckSceneMaterialGuards` pass (`MultiViewTests.log`): two cameras share the same final immutable material frame with independent View identity/viewport/depth; explicit stale camera falls back, foreign and invalid inputs reject; all five canonical semantics reject external providers and input batches atomically.
- `CheckSceneMetadataReuse` passes (`EffectiveRevisionTests.log`), 48 frames: camera motion, selected radiance changes, rename/focus and unselected-light edits keep bridge model-preparation count constant and warmed BVH rebuild/refit zero. The fixture reuses one whole local packet and builds zero prepared inputs; consequently local-record counters are zero, not evidence of a miss. Effective Scene key changes only when selected lighting values change. The original mistaken local-record assertion and retry diagnostics are retained in `MetadataReuseTests.log`/`MetadataReuseTestsRetry.log`.
- Real gated publication and old-RHI-graph tests pass in the same scene_render_tests executable. Main validation failure retains all three geometry/camera changes and token for atomic retry. Pure metadata Render failure is observed and blocks builds; cleanup and reattachment recover. No artificial dispatch-allocation failure was injected.
- `CheckOwnedCameraLightRoutes` and `CheckOwnedOffscreenShadow` pass in both Standard/Reversed depth modes (`OwnedOffscreenTests.log`). Forward/Deferred and ordinary/instanced parity keep existing tolerances. A parented caster outside the main frustum contributes to shadow views and produces 0.145098 maximum receiver contrast; parent motion, lens changes, near-pole direction and cast-disable/delete paths execute successfully.
- Viewer CSV appends `index_rebuilds,index_refits`; bridge exposes a Main-only monotonically increasing model-preparation counter. No timing, atomics or allocations were added to this diagnostic path. Model detach/removal compatibility bookkeeping now clears/erases in place instead of allocating a full projection after committing the logical edit.
- Camera helper constructs its rigid basis directly, preserving the supplied eye exactly instead of applying a general matrix inverse. Repeated image checks still exceed max-2 tolerance: current Debug Showcase moving max 94 / mean 0.00149413665, 48 of 1048320 pixels exceed 2; Model static max 3 / mean 0.000590020576, 4 of 1296000 pixels exceed 2. Showcase static max 2 / mean 0.0000753586691 passes. The earlier moving result before this refinement was max 109 / mean 0.0047771673. Thresholds have not changed. Current data is `CandidateCaptures/PrecisionDifferences.json`; further trajectory evidence and final cross-config captures remain pending.
- Updated SceneManagement, SourceLayout and NativeAssets documentation for hierarchy, scene-owned camera/light inputs, v4/source-v2 migration, save and interaction. Groups 4-10 checkboxes reflect implemented code and focused checks; complete V1 mapping/full suites/performance/UI evidence remain in group 11 until collected.

## Final implementation and test additions

- Added combined real SaveAsync snapshot A/live B and injected IO failure coverage, preserving visible save status and live edits. Empty, group-only and camera/light-only hierarchy saves now run actual native disk SaveAs and reload with no models/assets or implicit defaults.
- Added a gated pending-model hierarchy fixture that edits parent/camera/light, deletes a subtree and reuses a slot before releasing IO. Completion preserves the surviving world/local values and selection, and does not revive deleted nodes.
- Extended owned Forward/Deferred mixed hierarchy tests with byte-identical None/Linear/BVH results and model-only group counts after parent motion.
- ModelViewer no-input or captured input no longer recalculates and overwrites an externally edited Far plane; dolly publishes its camera pose, focus and lens together only on an actual gesture.
- The complete first performance matrix exposed CPU regressions. Full camera/viewport identity now lives at the View cache entry, whose scope revision invalidates downstream short keys. FinalKeyDebugBuild.log/FinalKeyReleaseBuild.log passed, followed by FinalCombinationTests 8/8 and FinalDeliverydebugCTest 62/62. Final Release and screenshots are collected separately in the handoff.
- Both complete 96-run A/B matrices and 3000-frame continuous runs are retained. Final V3 data is in performance-results.md and MainRun/PerformanceMatrixFinal. Eight final cells exceed a review trigger; raw samples and absolute timings are retained. A separate baseline continuous run has byte-identical per-frame GPU allocations, descriptor/PSO/cache counts, cumulative constant writes, visible items and main/shadow draw workload across all 3000 frames (ContinuousComparison.json). This does not assert a global GPU memory plateau.

## Final V1 evidence map and status

Test source roots below are Source/Tests. Full-suite logs include the executable output in the matching CTestDetails.log. “Covered” means the listed checks ran successfully in the completed suites; any narrower coverage or pending extension is stated explicitly.

| ID | Test function / fixture | Evidence and boundary |
| --- | --- | --- |
| N01 | Scene/SceneNodeTests.cpp CheckNodeIdentityAndChanges | CPU identity/kind enumeration, stale/foreign, reuse, no-op and changes; scene_management. |
| N02 | CheckDocumentInstallation | 4096 reverse-order chain, cycle/missing parent rejection; scene_management. |
| N03 | CheckHierarchyOracleAndTransactions | Independent matrix oracle, KeepLocal/KeepWorld and invalid-inverse atomicity; scene_management. |
| N04 | CheckInheritedEnabledAndRemoval; CheckCameraAndLightValues | Mixed inherited enable, model-only visibility, selection deletion, failed edit state preservation. |
| C01 | CheckCameraAndLightValues; CheckDefaultLightValues | Independent analytic pose and invalid lens/basis/overflow checks; Scene remains RHI-free. |
| C02 | Renderer/SceneInstanceTests.cpp CheckSceneViewSelection; DeferredRenderTests.cpp CheckOwnedCameraLightRoutes | Two cameras/View identities share immutable frame; viewport/depth/FOV values; inherited-camera actual GPU parity. |
| C03 | CheckSceneViewSelection; CheckOwnedCameraLightRoutes; CheckGatedOldSceneGraph | Explicit stale fallback/foreign rejection; no-camera clear pixel and recovery; initial empty token. |
| C04 | CheckSceneNavigation; CheckNavigationPrecision; ModelRenderTests.cpp CheckCameraInput | External edit/no-input/captured-input preservation, orbit/pan/dolly/fit, inverse failure; 600 legacy-orbit operations maximum eye error 2.71127e-05 and forward error 1.11064e-06. |
| L01 | SceneNodeTests.cpp light/pose tests; DeferredRenderTests.cpp owned routes and offscreen shadow | Numeric light validation, sign, parent propagation and known receiver shadow pixel. |
| L02 | CheckSceneFrameTokens; CheckSceneMetadataReuse; owned GPU routes | Selected/unselected effective values and cache keys, disabled/deleted direct light, separate ambient/unlit paths. |
| L03 | DeferredRenderTests.cpp CheckOwnedOffscreenShadow; ShadowRenderTests.cpp existing previews | Actual receiver contrast, cast-off/removal, preview/neutral behavior. Original shadow resolution retirement assertion has an unresolved intermittent Debug failure described below. |
| P01 | Assets/SceneSourceTests.cpp CheckLegacyVersions; Scene/SceneTests.cpp | Native v1/v2/v3 migration, nested migration diagnostics retained, stable IDs and full affine/material data, v4 roundtrip. |
| P02 | SceneSourceTests.cpp CheckSourceNodes/CheckInvalidSourceNodes | Source v1/v2/reflected/native, children-before-parent and invalid payload/selection/transform cases; shipped scenes reimported as v4. |
| P03 | CheckNodeOnlySnapshot; SceneViewerTests.cpp CheckSaveReload; new CheckSnapshotIsolationAndFailure | Node-only hierarchy roundtrip and existing model SaveAs pixels pass. Combined model/tree/two-camera/two-light snapshot isolation passed in FinalCombinationTestsDetails.log. |
| P04 | CheckPendingMaterialEdits; SharedAssetPublicationTests.cpp; SceneInstanceSnapshot.cpp | Pending/failed/unpersistable material and source-less model rejection, model/material/texture dependency rebase and pin tests; node-only save. CheckEmptyAndNodeOnlySave saves and reloads empty, group-only, and camera/light hierarchy through the actual asset IO service; exact serialized equality, zero models/assets and no implicit defaults passed. |
| P05 | PublicationTests.cpp CheckSceneImporterCache; SharedAssetPublicationTests.cpp; AssetTool tests | Importer revision 2/native schema 4, repeated import cache hit, edited focus changes revision, native upgrade; model wrapping and optional dependencies. |
| A01 | CheckLoadingEdits; CheckClosePending; CheckPendingMaterialEdits; new CheckPendingHierarchy | Existing gated model/data/material edits, slot reuse and partial failure pass. CheckPendingHierarchy passed the combined parent/camera/light/subtree gate and preserved the surviving model, tombstones, replacement and partial failure. |
| A02 | SceneViewerTests.cpp CheckSnapshotIsolationAndFailure | New IO-write gate captures A, edits live B, then writes/loads A; injected write failure checks visible status and no rollback. Passed in FinalCombinationTestsDetails.log, including visible failure status and unchanged live B. |
| F01 | SceneRenderTests.cpp CheckGatedScenePublications | Real Render gate, P1/F1/P2/F2, distinct geometry/camera/light, red/blue actual pixels and tokens. GatedSceneTestsRetry.log; final suites. |
| F02 | CheckSceneFrameTokens; CheckGatedOldSceneGraph | Exact stale token and old attachment epoch rejection. |
| F03 | CheckGatedOldSceneGraph; CheckRetainedFrames; existing cpu_frame_ownership/cpu_frame_pipeline/cpu_frame_viewer | Delayed RHI preparation retains old graph/constants/resources while Main deletes scene; existing frame-lead configurations retained. |
| F04 | CheckGatedScenePublications; CheckFailedMetadataPublication | Main validation failure retains delta/token and retry succeeds. Camera-only Render failure yields scene receipt/error, blocks builds, Close+reattach recover. Artificial TaskSystem dispatch allocation failure was not injected. |
| F05 | CheckNodeOnlySnapshot; CheckGatedOldSceneGraph; CheckFailedMetadataPublication; Viewer acceptance | First empty serial=1, no-presentation publication/deletion/close, old prepared graph, reload/epoch; existing window minimize/restore pipeline checks. |
| M01 | CheckSceneMaterialGuards | Five provider negative cases, Scene/Global atomic batches, Frame/View/pass rejection, retained unbound material tests. |
| M02 | CheckSceneMetadataReuse | 48 warmed camera/light/rename/focus/unselected edits: bridge model preparation stable, BVH rebuild/refit 0, one local packet reused and zero prepared inputs. Original local-record assertion was corrected to the actually exercised packet path; failure logs retained. |
| B01 | scene_spatial_visibility; CheckOwnedCameraLightRoutes; Scene hierarchy CPU tests | Existing None/Linear/BVH equivalence plus mixed typed publication and parent transforms. The mixed hierarchy fixture now runs None/Linear/BVH with byte-identical pixels after parent motion; exactly three model groups and zero unbounded groups. |
| B02 | CheckSceneMetadataReuse; ScenePerformance.py; V3 matrix | Warmed metadata updates keep model preparation and geometry counters unchanged; existing exact ordinary/instance pixels and draw reduction retained. The complete final V3 matrix and baseline continuous comparison are recorded below. |
| R01 | CheckOwnedCameraLightRoutes | Both depth modes × Forward/Deferred × CSM on/off × ordinary/instanced, inherited camera/light, masked/blended/mirrored models, actual pixels and zero validation. |
| R02 | CheckOwnedOffscreenShadow | Parented caster outside main frustum contributes to shadow views; max receiver contrast 0.145098; parent motion/lens/pole/cast/remove paths. |
| U01 | SceneViewerNodes.cpp + scene_viewer_controls + GUI capture | Typed API/control checks and visual tree capture; no automated native mouse interaction across every new form/button. GuiCapture/SceneTree.png visually inspected: all five kinds and hierarchy indentation visible, model-only statistics correct; rc=0 and zero validation. |
| U02 | SceneViewerTests.cpp/ModelRenderTests.cpp and Viewer acceptance | Existing save/reload pixel max <=1/255, input/fit/capture; CLI light and camera benchmark now edit scene. Cross-version screenshots completed; 8/10 fail the fixed maximum pixel tolerance, with raw table below. |


Final Debug/Release suites: 62/62 each. Final style/naming/boundary/strict-spec/whitespace checks all rc=0. Cross-version images: 2/10 pass both fixed thresholds, 8/10 fail max; final performance: eight cells retain review triggers. See review-handoff.md for full tables, earlier failures and exact snapshot. Task 11.4 remains open for unexecuted dispatch-allocation and complete native mouse probes; group 13 review remains deferred. No staging, commit or archive.

## Subsequent authorized quality audit — 2026-09-12

The user subsequently authorized independent review and in-scope repair, while deferring the five previously listed follow-ups. Three P2 defects were independently confirmed, repaired and closed by the original reviewer; Debug and Release targeted regressions each passed 19/19, and reviewer CPU verification passed 2/2. See [audit-report.md](audit-report.md) for precise scope, dispositions, current validation and the final audit snapshot. Earlier whole-change image/performance/full-suite data above retain their original pre-audit binary identity; they were not rerun or relabeled as post-audit evidence. No staging, commit or archive occurred.
