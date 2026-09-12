# Implementation handoff — scene-owned-cameras-and-lights

This is the implementer's evidence handoff, not an independent review or approval. The user explicitly deferred group 13. No staging, commit or archive has been performed. Implementation and the final GPU/test/performance/capture runs are complete. This is not an all-pass acceptance: image thresholds and performance review triggers remain, and specific unexecuted probes are listed below.

## Baseline and environment

Baseline and current HEAD: `ee585cc88fd60a22a29dc2103cd4dafe6d234221`. The previous executor's residual Source changes were preserved then reverted after the user confirmed that executor had stopped. The proposal artifacts were retained. Current source changes belong to this implementation.

Evidence root: `out/SceneOwnedCamerasAndLights/MainRun/`. Baseline is a separate `git archive` of HEAD with its own built binaries, generated native content, source assets, shaders and configs. The locked dependency sources are shared through a junction. `BaselineInputs.json`, `BaselineBinaryContentHashes.json` and `CandidateBenchmarkBinaryHashes.json` describe the frozen inputs. Debug/Release builds use the repository build helpers, Tracy OFF, RenderDoc ON, existing D3D12 validation; GPU NVIDIA RTX 5080, driver 32.0.15.9186. Standard/Reversed depth tests both run; shipped Scene.json uses Reversed Z. Performance uses the same explicit Showcase, CSM 1024, no VSync or GUI, default frame lead, and no simultaneous GPU jobs.

## D1–D12 implementation map

| Decision | Implementation |
| --- | --- |
| D1 unified nodes | Scene public SceneNode.h; private SceneNode.cpp, SceneInternal.h. Exactly one optional payload or Group. FSceneModel is a derived compatibility/renderer transfer value. |
| D2 hierarchy and changes | SceneMutation.cpp, SceneHierarchy.cpp, Scene.cpp. Staged subtree/world/pose validation, iterative graph traversal, complete Handle change keys, transactional commit, settings cleanup. |
| D3 camera | SceneCamera.cpp and SceneNavigation.cpp. CPU pose/lens authority, world-space gestures via scene edits, atomic transform+lens changes; SceneFrame.cpp resolves camera View with viewport/depth. |
| D4 lighting | SceneLight.cpp, SceneDefaults.cpp, SceneFrame.cpp Lighting; explicit settings choose one directional and one environment light, absent/disabled lights contribute zero. |
| D5 publication | ScenePublication.h, SceneBridgeMetadata.cpp, SceneMaterialPublication.cpp, SceneMetadata.cpp, SceneFrame.cpp. Full identity/epoch/serial/revision, single geometry+metadata admission, exact applied token validation, scene receipt and failure cleanup. |
| D6 material semantics | MaterialProviders.cpp, SessionMaterialFrame.cpp, SessionMaterialBinding.cpp, SceneFrame.cpp; five protected builtins, atomic rejection of conflicting batches, retained unbound fixtures. |
| D7 incremental boundary | SceneBridge.cpp filters model masks and editable material handles; SceneMetadata.cpp does not call geometry OnChanged. EffectiveSceneInputs compares lighting/custom values; SessionViewCache.cpp keys actual View values. |
| D8 persistence | SceneManifest.cpp, SceneLegacy.cpp, SceneMigration.cpp; SceneJson.cpp source v2; AssetPublication.cpp importer revision 2. Root v4 retains native v1–v3 migration, preserves nested material records and affine matrices. |
| D9 runtime/save | SceneInstanceLoading.cpp installs all nodes before model IO; SceneInstanceMaterials.cpp binds completion to epoch/full Handle. SceneInstanceSnapshot.cpp snapshots authoritative nodes/settings and visits all FAssetRef values for rebase. |
| D10 viewers/UI | SceneViewerNodes.cpp and Gui wrappers; SceneViewerControls.cpp/ModelViewerPlugin.cpp operate scene nodes. ViewerShadows.cpp applies CLI override once and edits the selected node; ViewerFramePipeline.cpp freezes seed immediately before submission. |
| D11 module boundaries | Scene owns data/graph/math only, Renderer owns bridge/frame/RHI integration; Gui backend alone includes ImGui. SceneManagement.md, SourceLayout.md and NativeAssets.md document the boundary. |
| D12 verification | Existing targets extended, same D3D12 test infrastructure and compatibility assertions. Full suites, gated tests, captures, A/B runs and remaining limitations are listed below; group 13 remains untouched. |

## V1 evidence map

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

## Self-check boundaries

Application ShadowLight only remains as immutable command-line input plus a one-shot applied flag. Orbit yaw/pitch/pivot values are local variables derived from current scene pose; plugin members do not maintain a second authoritative camera. The compatibility FSceneModel projection derives world/effective visibility from nodes; FSceneInstance Models is a const compatibility identity association. Mutable material instances still require scanning their explicit bridge subscription set; camera/light-only edits do not scan all immutable models. Metadata snapshots copy the camera/light maps, not model geometry. Material Scene key uses selected effective values and attachment identity, not publication serial. Full camera/viewport qualifiers are compared in FViewEntry before incrementing the View scope revision; downstream draw keys retain the short View identity rather than duplicating those qualifiers. Scene frame resolution replaces bound legacy builtin lighting with explicit selected values; absent lights stay zero. No RHI backend or GPU wait/validation behavior was changed.

## Final evidence index

The sections below supersede intermediate pending statements in implementation.md. Raw commands, captures and CSVs remain under the evidence root. Final source/artifact hashes and the exact file list follow the check summary.

## Evidence before the final performance rerun

`FinalKeyDebugBuild.log` and `FinalKeyReleaseBuild.log` passed. `FinalCombinationTests.log`/`FinalCombinationTestsDetails.log`: 8/8 passed (six GPU targets plus fixture dependencies), 50.60 s; scene_runtime_instance, scene_viewer_controls, deferred_rendering, cascaded_shadow_rendering, scene_rendering and model_rendering. This run includes the newly added async save IO gate/error, empty/group/node-only real disk roundtrips, pending mixed hierarchy edits, and mixed None/Linear/BVH actual pixel checks.

The first complete 96-run matrix plus 3000 measured-frame continuous case is retained under `PerformanceMatrix/` and `InitialPerformanceReport.md`. All 97 runs returned 0, were ready, had unique completed GPU submission IDs, expected samples and zero validation errors. Candidate geometry rebuild/refit remained zero. Debug moving-light regressed +8.82%/+8.60% frame mean and +9.33%/+9.19% pipeline mean for Forward/Deferred. Identical visible items, draw/caster counts, upload bytes and cache reuse counters rule out changed geometry workload or broad invalidation in these cells. Material and shadow preparation CPU times increased while GPU times did not. This is evidence of CPU cost, not a proven attribution to one instruction.

A focused optimization moved full camera/viewport identity comparison out of repeated material scope keys into the per-View cache entry. The scope revision still changes for every effective identity/viewport/value change. The final full matrix runs against `FinalCandidateBinaryHashes.json` in `PerformanceMatrixFinal/`; the first matrix is not discarded or renamed as final. Remaining trigger lines are reported after the rerun.

## Final performance and long-run interpretation

The final 97-run matrix completed successfully: 96 interleaved A/B runs (600 measured samples each) and 3000 measured Debug Forward camera+light frames. Every run returned 0, was scene-ready, reported zero validation errors, and had one unique GPU sample per completed submission. All candidate rebuild/refit counters were zero. `performance-results.md` contains every individual run, all 16 comparison cells, CPU/GPU mean/P95 tables and resource ranges; `PerformanceMatrixFinal/` retains the per-frame data.

Eight cells retain at least one review trigger; these are unresolved performance limitations, not passes against the trigger lines. Debug moving-light frame/pipeline means are +5.34%/+5.93% Forward and +7.88%/+8.44% Deferred (Deferred pipeline P95 +10.40%). Release Forward large-camera frame mean/P95 are +11.51%/+13.03%; other Release triggers are listed with absolute times in the report (typically 0.04–0.11 ms pipeline differences). No samples were removed. Pipeline prepare is a sum of render preparation plus deferred family/fullscreen preparation, not simply wall-clock frame duration. Geometry/caster coverage, uploads and reuse remain equivalent; the data locates the difference on CPU preparation, but does not prove a single remaining cause. The two-file View-key optimization preserved semantic tests and reduced the Debug Forward light-motion gap; it did not remove every trigger. No CSM, validation, frame-lead or workload settings were reduced.

`BaselineContinuous/` is a separate old-binary 3000-frame run with the same camera+light command. `ContinuousComparison.json` compares each measured frame. GPU allocation bytes, descriptor count, PSO count and cached plan/input counts match baseline/candidate. Both grow from 35,876,864 to 43,741,184 GPU bytes; descriptors stay 115 and PSOs 5. This shows no added GPU footprint in the measured path, but neither run reaches a plateau within 3000 frames. Existing MaterialConstantLimits remain 4096 cached blocks/16 MiB and 512 prepared blocks; page reclamation still follows expired weak scopes and native ownership. Those cache limits are not a measured global GPU plateau, and this handoff does not claim one. `constant_bytes_written` is a cumulative device counter (D3D12Buffers.cpp), not per-frame resident memory. Old-frame gated tests pass and no RHI retirement/wait behavior was modified.

## Final full-suite results and retained earlier failures

`FinalDeliverydebugCTest.log`/`FinalDeliverydebugCTestDetails.log`: 62/62 passed, rc=0, 328.33 s. `FinalDeliveryreleaseCTest.log`/`FinalDeliveryreleaseCTestDetails.log`: 62/62 passed, rc=0, 201.36 s. Both final suites use the final short-View-key binaries and all added tests; they include the desktop/GPU and RenderDoc targets. `FinalPostPerformanceResults.json` records commands and return codes.

Earlier negative runs remain available: the baseline Debug suite was 61/62 because the original scene_moving_camera fixture was not ready; the first candidate full Debug/Release runs failed the same moving fixture, and Debug additionally failed the unchanged shadow GPU-allocation assertion. The moving test now explicitly imports Showcase-v2 with its own camera fixture (FOV 0.8), uses the required 240-frame warmup, and preserves the original 190–210 visibility range, exact ordinary/instance pixels, draw-reduction and cache/upload assertions. Shipped Showcase is unchanged by that test calibration; baseline/candidate A/B both use shipped Showcase at 185–188 visible items. Probe and original failure logs are retained.

The shadow resolution test has only a diagnostic print added; its four frames and 8 MiB allowance remain unchanged. Two earlier full Debug runs exceeded the allowance after 2048→1024 replacement, including 84,807,680 bytes versus a 17,833,984 small-map baseline. Three isolated baseline/candidate pairs passed 6/6; the final affected-target run and final Debug/Release suites passed; `BaselineShadowRepetition1.log` through `BaselineShadowRepetition10.log` all passed independently. A source-level possible contributor is the existing asynchronous 2 ms Worker→RHI maintenance path: completed native fences do not by themselves prove the material cache maintenance task has run. This is a hypothesis, not an established root cause. The earlier intermittent failure remains a limitation for follow-up; no wait, retirement, validation setting or assertion was weakened to force a pass.

## Final cross-version captures and GUI inspection

All 12 executable capture runs (10 candidate comparisons and two new Sponza-moving baselines), plus the GUI capture, returned 0 with zero validation errors. Fixed 8-bit thresholds remain max <=2 and RGB mean <=0.1. Only the two Showcase-static comparisons pass both thresholds; 8/10 fail the maximum check. No tolerance was expanded. Scene captures use Forward/Reversed Z/CSM 1024, matching frozen baseline settings; both depth conventions and both pipelines are separately covered by real GPU semantic tests. Showcase/Model use 840 frames; Sponza uses 10840 frames with both versions' 10000-frame warmup.

| Candidate | max channel delta | RGB mean delta | pixels over 2 | result |
| --- | ---: | ---: | ---: | --- |
| debug-Showcase-static-candidate | 2 | 0.0000753587 | 0/1048320 | PASS |
| debug-Showcase-moving-candidate | 94 | 0.0014941367 | 48/1048320 | FAIL max |
| debug-Model-static-candidate | 3 | 0.0005900206 | 4/1296000 | FAIL max |
| debug-Sponza-static-candidate | 60 | 0.0010737815 | 6/1048320 | FAIL max |
| debug-Sponza-moving-candidate | 85 | 0.0204931064 | 192/1048320 | FAIL max |
| release-Showcase-static-candidate | 2 | 0.0000753587 | 0/1048320 | PASS |
| release-Showcase-moving-candidate | 94 | 0.0014941367 | 48/1048320 | FAIL max |
| release-Model-static-candidate | 3 | 0.0005900206 | 4/1296000 | FAIL max |
| release-Sponza-static-candidate | 60 | 0.0010737815 | 6/1048320 | FAIL max |
| release-Sponza-moving-candidate | 85 | 0.0204931064 | 192/1048320 | FAIL max |

`FinalCaptures/Results.json` contains the exact commands and baseline paths. `FinalCaptures/DifferenceDiagnostics.json` records largest-delta coordinates, raw RGB values, bounding boxes and baseline 3x3 per-channel contrast. For example, Showcase-moving maximum is at (1376,415), and Sponza-static at (378,503). Overall Sponza framing and lighting were visually inspected; the sparse differences are consistent with pose reconstruction and high-contrast raster boundaries, but that explanation does not turn the fixed maximum-threshold failures into passes. Independent 600-operation camera trajectory oracle measured max eye 2.71127e-05 and max forward 1.11064e-06; it does not prove the full 10840-operation Sponza trajectory. FOV, view/depth, selected radiance/sign, CSM direction and actual lit/shadow receiver behavior are separately covered by numeric and GPU tests.

`GuiCapture/SceneTree.png` is an actual GUI-on capture, not a mockup. The right panel visibly contains all five kinds, the model and camera indented under Group, three scene selections and correct one-model/one-group render statistics. The panel scrolls to creation and typed property controls below. `GuiCapture/Result.json` reports rc=0 and zero validation errors. Native mouse-driven automation across every new button/form was not performed; typed API/control, save and failure paths have tests.

## Remaining acceptance limits

- Eight cross-version image comparisons exceed the fixed maximum pixel threshold; the table above retains all results.
- Eight performance cells retain one or more V3 review triggers, with all repeated samples included. The remaining CPU difference is not fully attributed.
- Earlier full Debug shadow retirement assertion failures were intermittent; final suites pass, but an exact root cause was not reproduced in isolated old-binary repetitions.
- The 3000-frame GPU footprint is byte-for-byte baseline-equivalent and cache capacities are unchanged; a global GPU memory plateau beyond that window was not demonstrated.
- TaskSystem dispatch-allocation failure was not artificially injected, and native mouse-driven coverage of every new GUI form/button was not run. Main validation retry, metadata-only Render failure, IO write failure, old-frame lifetime and typed control behavior did run.

Implementation stops here under the user's instruction. Group 13 independent review, staging, commit and archive are not performed. A later review should assess these remaining acceptance limits using the fixed file and evidence snapshot rather than interpreting the implementation checkboxes as review approval.

## Final repository checks

`DeliveryCheckResults.json` records every command with rc=0. `DeliveryStyle.log`: 453 owned files and formatting passed. `DeliveryNaming.log`: semantic naming/local declarations passed for 281 translation units. `DeliveryBoundaries.log`: module dependency and header boundaries passed. `DeliveryOpenSpec.log` and `DeliveryAllSpecs.log`: this change and all current specs pass strict validation. `DeliveryWhitespace.log`: diff whitespace check passed. No source edits followed the final builds, GPU suites or benchmark binary freeze.

Task 11.4 remains unchecked for the explicitly unexecuted dispatch-allocation injection and full native GUI mouse replay. The other implementation/collection tasks are checked with the acceptance limitations above; the three reviewer-only tasks stay unchecked. No independent approval is implied.

<!-- FINAL FILE LIST -->
## Exact final change file list

HEAD `ee585cc88fd60a22a29dc2103cd4dafe6d234221`, branch `main`. 80 tracked modified files and 42 untracked files. Hashes for this exact snapshot are in `out/SceneOwnedCamerasAndLights/MainRun/FinalChangeFiles.json`; no files are staged. The untracked OpenSpec files include the proposal created before implementation.

| State | Path |
| --- | --- |
| tracked modified | `Source/Applications/AssetTool/Private/AssetCommands.cpp` |
| tracked modified | `Source/Applications/Viewer/Private/ViewerApplication.h` |
| tracked modified | `Source/Applications/Viewer/Private/ViewerBenchmark.cpp` |
| tracked modified | `Source/Applications/Viewer/Private/ViewerFrame.cpp` |
| tracked modified | `Source/Applications/Viewer/Private/ViewerFramePipeline.cpp` |
| tracked modified | `Source/Applications/Viewer/Private/ViewerShadows.cpp` |
| tracked modified | `Source/Plugins/ModelViewer/Private/ModelViewerPlugin.cpp` |
| tracked modified | `Source/Plugins/ModelViewer/Public/Hyperion/ModelViewer/ModelViewerPlugin.h` |
| tracked modified | `Source/Plugins/SceneViewer/CMakeLists.txt` |
| tracked modified | `Source/Plugins/SceneViewer/Private/SceneViewerControls.cpp` |
| tracked modified | `Source/Plugins/SceneViewer/Private/SceneViewerGui.cpp` |
| tracked modified | `Source/Plugins/SceneViewer/Private/SceneViewerInternal.h` |
| untracked | `Source/Plugins/SceneViewer/Private/SceneViewerNodes.cpp` |
| tracked modified | `Source/Plugins/SceneViewer/Private/SceneViewerPersistence.cpp` |
| tracked modified | `Source/Plugins/SceneViewer/Private/SceneViewerPlugin.cpp` |
| tracked modified | `Source/Plugins/SceneViewer/Public/Hyperion/SceneViewer/SceneViewerPlugin.h` |
| tracked modified | `Source/Runtime/AssetImport/Private/Adapters/SceneJson.cpp` |
| tracked modified | `Source/Runtime/AssetImport/Private/AssetPublication.cpp` |
| tracked modified | `Source/Runtime/Gui/Private/Adapters/Gui.cpp` |
| tracked modified | `Source/Runtime/Gui/Public/Hyperion/Gui/Gui.h` |
| tracked modified | `Source/Runtime/Reflection/Private/Record.cpp` |
| tracked modified | `Source/Runtime/Reflection/Public/Hyperion/Reflection/Record.h` |
| tracked modified | `Source/Runtime/Renderer/CMakeLists.txt` |
| tracked modified | `Source/Runtime/Renderer/Private/CascadedShadowMap.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/ForwardRenderPipeline.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/MaterialProviders.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/RenderSceneClient.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/RenderSceneInternal.h` |
| tracked modified | `Source/Runtime/Renderer/Private/RenderSession.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneBridge.cpp` |
| untracked | `Source/Runtime/Renderer/Private/SceneBridgeMetadata.cpp` |
| untracked | `Source/Runtime/Renderer/Private/SceneFrame.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneInstance.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneInstanceInternal.h` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneInstanceLoading.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneInstanceMaterials.cpp` |
| untracked | `Source/Runtime/Renderer/Private/SceneInstanceNodes.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneInstanceSnapshot.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneMaterialPublication.cpp` |
| untracked | `Source/Runtime/Renderer/Private/SceneMetadata.cpp` |
| untracked | `Source/Runtime/Renderer/Private/SceneNavigation.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneRemoval.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SceneRenderPipeline.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SessionMaterialBinding.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SessionMaterialFrame.cpp` |
| tracked modified | `Source/Runtime/Renderer/Private/SessionMaterialsInternal.h` |
| tracked modified | `Source/Runtime/Renderer/Private/SessionViewCache.cpp` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/CascadedShadowMap.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/ForwardRenderPipeline.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/MaterialFrame.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/MaterialProviders.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/RenderPlugin.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/RenderPrimitive.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/RenderScene.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/RenderSession.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/SceneBridge.h` |
| untracked | `Source/Runtime/Renderer/Public/Hyperion/Renderer/SceneFrame.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/SceneInstance.h` |
| untracked | `Source/Runtime/Renderer/Public/Hyperion/Renderer/SceneNavigation.h` |
| untracked | `Source/Runtime/Renderer/Public/Hyperion/Renderer/ScenePublication.h` |
| tracked modified | `Source/Runtime/Renderer/Public/Hyperion/Renderer/SceneRenderPipeline.h` |
| tracked modified | `Source/Runtime/Scene/CMakeLists.txt` |
| tracked modified | `Source/Runtime/Scene/Private/Scene.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneCamera.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneDefaults.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneHierarchy.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneInternal.h` |
| untracked | `Source/Runtime/Scene/Private/SceneLegacy.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneLight.cpp` |
| tracked modified | `Source/Runtime/Scene/Private/SceneManifest.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneMigration.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneMutation.cpp` |
| untracked | `Source/Runtime/Scene/Private/SceneNode.cpp` |
| tracked modified | `Source/Runtime/Scene/Public/Hyperion/Scene/Scene.h` |
| untracked | `Source/Runtime/Scene/Public/Hyperion/Scene/SceneCamera.h` |
| untracked | `Source/Runtime/Scene/Public/Hyperion/Scene/SceneLight.h` |
| tracked modified | `Source/Runtime/Scene/Public/Hyperion/Scene/SceneManifest.h` |
| untracked | `Source/Runtime/Scene/Public/Hyperion/Scene/SceneNode.h` |
| tracked modified | `Source/Tests/Assets/PublicationTests.cpp` |
| untracked | `Source/Tests/Assets/SceneSourceTests.cpp` |
| tracked modified | `Source/Tests/Assets/SharedAssetPublicationTests.cpp` |
| tracked modified | `Source/Tests/CMakeLists.txt` |
| tracked modified | `Source/Tests/Integration/FramePipelineAcceptance.py` |
| tracked modified | `Source/Tests/Integration/ScenePerformance.py` |
| tracked modified | `Source/Tests/Renderer/DeferredRenderTests.cpp` |
| tracked modified | `Source/Tests/Renderer/ModelRenderTests.cpp` |
| tracked modified | `Source/Tests/Renderer/SceneInstanceTests.cpp` |
| tracked modified | `Source/Tests/Renderer/SceneRenderTests.cpp` |
| tracked modified | `Source/Tests/Renderer/SceneViewerTests.cpp` |
| tracked modified | `Source/Tests/Renderer/ShadowRenderTests.cpp` |
| tracked modified | `Source/Tests/Renderer/SharedAssetGpuTests.cpp` |
| untracked | `Source/Tests/Scene/SceneNodeTests.cpp` |
| tracked modified | `Source/Tests/Scene/SceneTests.cpp` |
| tracked modified | `Source/Tests/Serialization/RecordTests.cpp` |
| tracked modified | `assets/Scenes/Shadows.json` |
| tracked modified | `assets/Scenes/SharedAssets.json` |
| tracked modified | `assets/Scenes/Showcase.json` |
| tracked modified | `assets/Scenes/Sponza.json` |
| tracked modified | `docs/NativeAssets.md` |
| tracked modified | `docs/SceneManagement.md` |
| tracked modified | `docs/SourceLayout.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/.openspec.yaml` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/design.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/implementation.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/performance-results.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/proposal.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/review-handoff.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/source-format-example.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/bounded-cpu-frame-pipeline/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/cascaded-shadow-maps/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/material-parameter-binding/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/scene-cameras/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/scene-lights/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/scene-management/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/scene-runtime-instance/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/scene-viewer/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/scene-visibility/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/specs/static-model-rendering/spec.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/tasks.md` |
| untracked | `openspec/changes/scene-owned-cameras-and-lights/verification-plan.md` |
| tracked modified | `tools/BatchPlannerComparison.py` |
| tracked modified | `tools/MeasureShadows.py` |

## Subsequent authorized quality audit — 2026-09-12

The user subsequently authorized independent review and in-scope repair, while deferring the five previously listed follow-ups. Three P2 defects were independently confirmed, repaired and closed by the original reviewer; Debug and Release targeted regressions each passed 19/19, and reviewer CPU verification passed 2/2. See [audit-report.md](audit-report.md) for precise scope, dispositions, current validation and the final audit snapshot. Earlier whole-change image/performance/full-suite data above retain their original pre-audit binary identity; they were not rerun or relabeled as post-audit evidence. No staging, commit or archive occurred.
