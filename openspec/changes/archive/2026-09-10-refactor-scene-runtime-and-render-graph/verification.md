# Implementation and verification

Date: 2026-09-10. Baseline source: `b4c1f94928eeecf20ed1e80bb653923aa19ff2ff`.

## Delivered design

- Renderer `FSceneInstance` owns Main scene lifecycle, the `FScene`/`FSceneRenderBridge` composition, asynchronous manifest/model requests, generation-safe model bookkeeping and structured status. SceneViewer retains camera, input, UI and the original animation behavior. CPU Scene stays independent of Renderer/RHI; ModelViewer continues using `FModel`.
- Graphics passes declare graph resources, attachments, aspect load/store, viewport, reads and dependencies before deferred preparation. View and target descriptions are separate. RHI commands expose explicit color/depth/stencil attachments and resource transitions; the old flag-driven public path is removed.
- Compilation checks topology and content before native resolution or draw preparation. RAW/WAR/WAW plus explicit After edges use stable ordering. Regional validity, fractional pixel bounds, discard, foreign/stale handles, conflicting imports and sampled/write feedback have negative coverage. D3D12 implements load/store discard for color/depth/stencil, validated by real GPU readback after reinitialization.
- Shadow producers, forward, preview and GUI retain ordering and one normal frame coordination boundary. Empty draw lists retain attachment actions; segmented draws apply load first and store last. Shared draw ownership, native cache validation, fences and failed-Present recovery remain intact.
- Static prepared views retain sampled-read declarations in the existing bounded view cache. Dynamic collection reads frozen values and shared binding proofs directly, without copying parameter tables. Batch target signatures derive depth format from attachments.

## Baseline and evidence locations

The pre-change Debug build passed 48/48 tests with no skips (`out/SceneGraphBaselineDebug.log`). The saved baseline Viewer and its DLLs are in `out/SceneGraphBaseline/bin`; Viewer SHA-256 is `9606B4514F557DE23774730740B9E9F02F657F1F1D1103F8837D2D8F4B934CEB`.

Deterministic before images/logs and two warmed runs per static/moving, shadows-off/on combination are in `out/SceneGraphBaseline/Before`. The capture recipe uses 180 frames, hidden window, no UI and no VSync; moving capture uses the deterministic benchmark camera. All runs require zero D3D12 validation errors. Model fixtures exercise both glTF and GLB.

Build-local evidence is intentionally not committed as source data.

## Final validation

| Check | Result | Evidence |
|---|---|---|
| Debug build and CTest | 49/49 passed, no skips | `out/SceneGraphFinalContractDebugBuild.log`, `out/SceneGraphAcceptanceDebug.log` |
| Release build and CTest | 49/49 passed, no skips | `out/SceneGraphFinalContractReleaseBuild.log`, `out/SceneGraphAcceptanceRelease.log` |
| Release scene pixel repetition | 20 consecutive passes | `out/SceneGraphSceneRepeatedFinal.log` |
| Formatting and naming | all 206 translation units passed; edited units rechecked after follow-up changes | `out/SceneGraphNaming.log`, `out/SceneGraphNamingFinalDelta.log`, `out/SceneGraphReadNaming.log`, `out/SceneGraphFinalContractNaming.log` |
| Module boundaries | passed | `out/SceneGraphBoundariesVerified.log` |
| OpenSpec strict validation / diff whitespace | passed | `out/SceneGraphSpecVerified.log`, `git diff --check` |

Both configurations have RenderDoc enabled and Tracy disabled. GPU tests keep the D3D12 debug layer enabled on NVIDIA GeForce RTX 5080. Tests cover both Viewer acceptances, scene controls, moving camera, material/instance caches, independent shadow culling, actual shadow/preview pixels, native resource retention, frame failure recovery and submitted GUI/scene draws in RenderDoc. The final pixel diagnostic helper was rebuilt with the corresponding `scene_render_tests` targets before the acceptance runs (`out/SceneGraphSceneDiagnosticBuild.log` and `out/SceneGraphSceneDiagnosticReleaseBuild.log`).

One intermediate Release run reported a blue-channel pixel mismatch in `scene_rendering` before the final attachment-format cleanup. It did not reproduce in three immediate repeats, the final full suite, or 20 consecutive final-version repeats. The test now prints the caller line and actual/expected RGB on a mismatch. A root cause for that intermediate observation was not established; these passes are the measured evidence, not a claim that an intermittent issue was proven fixed.

## Deterministic images

All six final RGBA images match the baseline pixel-for-pixel: Scene, SceneNoShadows, SceneMoving, ScenePreview, ModelGltf and ModelGlb. Scene images are 1440x900 and model images 1280x720. Images/logs are in `out/SceneGraphBaseline/FinalCaptures`; comparison and binary hash are in `out/SceneGraphBaseline/FinalComparison.json`. Every capture completed with zero validation errors. The scene/depth-preview composition was also visually inspected.

## Warmed performance

Final comparison alternates the preserved old and final new Debug executables, then reverses execution order for the second repetition. Each run uses 200 warmup frames plus 400 measured frames, no VSync, 2048 shadow maps, fixed or deterministic moving camera, and no concurrent build/test/GPU workload. Each run validates exact CPU/GPU sample coverage, scene readiness, nonempty shadow work, zero failed items, stable pipeline/descriptor counts and bounded native allocation. These are local Debug measurements, not a universal performance guarantee.

| Camera / shadows | Old frame ms | New frame ms | Change | Old preparation ms | New preparation ms |
|---|---:|---:|---:|---:|---:|
| Static / off | 1.094 | 1.174 | +7.3% | 0.488 | 0.504 |
| Static / on | 2.288 | 2.527 | +10.5% | 1.450 | 1.535 |
| Moving / off | 2.928 | 3.349 | +14.4% | 2.250 | 2.569 |
| Moving / on | 7.759 | 8.700 | +12.1% | 6.599 | 7.308 |

Initial results were worse because read declaration copied parameter tables and scanned static views every frame. The delivered version reads frozen values directly, deduplicates array traversal and retains static read declarations with the prepared view. Residual CPU overhead remains from explicit declaration/validation and read collection; it is reported above rather than described as a performance improvement. No validation, GPU lifetime checks, material behavior or rendering features were disabled to reduce it. Raw runs: `out/SceneGraphBaseline/FinalInterleaved/Summary.json` and associated CSV/logs. Earlier measurements are preserved in `After`, `Optimized` and `Interleaved`.

## Scope and delivery state

No new rendering algorithm, gameplay framework, data format, CLI migration, MRT/MSAA/compute/transient resource system was introduced. The active OpenSpec change is ready for user acceptance; it has not been archived or committed. Architecture references: `docs/SceneManagement.md`, `docs/RenderGraph.md`, `docs/RenderPrimitives.md` and `docs/CascadedShadows.md`.

## Audit P2 repairs (2026-09-10)

The independent audit identified four correctness findings, all repaired and closed by the original reviewer:

- SC-02: an already closed scene now returns before touching external dependencies. A regression test closes it, destroys its dependency fixture, calls Close again and destroys the scene.
- SC-01: per-handle errors validate the generation and include the associated asset-load failure before falling back to render-bridge errors. Tests cover healthy/failed models, removal and slot reuse.
- RG-01: graph copy/move construction and assignment guard both participating graphs before touching active compilation storage. Moves give the source a fresh identity; tests exercise all source/target operations inside preparation and valid transfers outside compilation.
- RG-02: `IRHITexture::GetInfo` exposes immutable physical base-level extent and depth format without native handles. Direct and deferred imports verify that description after resolution and before draw preparation. CPU and real D32 tests reject mismatched dimensions while existing GPU pixel/retention coverage remains intact.

Repair and final naming-only delta re-review: `out/QualityAuditSceneGraph/P2ReReview.md`. The initial frozen repair snapshot is `P2Snapshot.json`; `P2FinalSnapshot.json` records the final state. The added verification text does not change reviewed source behavior.

Final repair validation:

| Check | Result | Evidence in out/QualityAuditSceneGraph |
|---|---|---|
| Debug build and full CTest | 49/49 passed, no skips | P2FinalDebugBuild.log, P2DebugTests.log |
| Release build and full CTest | 49/49 passed, no skips | P2ReleaseBuild.log, P2ReleaseTests.log |
| Final abstract mock rename | Release target rebuilt, render_resources passed | P2FinalReleaseDeltaBuild.log, P2FinalReleaseDeltaTests.log |
| Targeted correctness tests | 5/5 passed | P2TargetedDebug.log |
| Semantic naming | all 206 units checked; sole abstract mock prefix error corrected and its unit passed | P2Naming.log, P2NamingFinalDelta.log |
| Formatting / module boundaries | passed | P2FinalStyle.log, P2Boundaries.log |

No performance optimization was attempted in these repairs, per the user's requested ordering. The performance figures and deterministic screenshots above belong to the pre-repair implementation measurements; they were not recaptured or remeasured for this repair round. Current Viewer acceptance and moving-camera tests passed in both configurations. The historical intermittent scene pixel observation still has no established root cause. No archive or commit was performed.

## Archive disposition (2026-09-10)

The user requested OpenSpec archival and Git commit after the four P2 repairs passed re-review and final Debug/Release verification. The earlier delivery-state statements describe their original checkpoints. This change is now archived under `2026-09-10-refactor-scene-runtime-and-render-graph`, with all five capability deltas synchronized to main specifications and all 16 tasks complete. The independent P2 repair review is retained in [independent-review.md](independent-review.md).

Performance optimization remains explicitly deferred; archive and commit do not imply that the measured CPU regression or historical intermittent pixel observation has been resolved. The final source is unchanged from the verified repair snapshot. This archival step changes specifications and records only, so no redundant build/GPU test run was required.
