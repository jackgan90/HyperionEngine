# P2 repair re-review

Date: 2026-09-10. Scope: original findings RG-01, RG-02, SC-01, SC-02 and direct effects of their repairs only. No performance optimization review in this round. Reviewer made no source or Git changes.

Candidate: out/QualityAuditSceneGraph/P2Snapshot.json, SHA256 29ab2be1c19720240bbf6c3d132cce3606257ee115ada0c160b4a2c6433ff927. The 12 repair_files and affected call paths/tests/docs were inspected. All files in the frozen snapshot still matched their recorded hashes during review.

## Findings disposition

| Finding | Decision | Repair and evidence |
| --- | --- | --- |
| RG-01 compilation mutation through assignment | Closed | RenderGraph.cpp:87-126 implements guarded copy/move construction and assignment. Both source and destination are checked before storage changes, including active-source moves/copies; no rejected operation releases the active pass/callback. Copy assignment prepares a local copy before replacing the target; moving transfers graph handles and gives the source a new identity. GraphTests CheckCompilationMutation now checks all six relevant source/target copy/move paths; CheckGraphMoves checks destination usability and moved-from identity. Current Ninja Debug graph_tests independently passed. |
| RG-02 unverified physical texture extent | Closed | RenderGraphResources.cpp:38-45 checks resolved texture base width, height and depth format before any pass draw Prepare. The same path handles direct and deferred resources. IRHITexture::GetInfo supplies engine-owned immutable metadata; D3D12Resources.h:49 derives dimensions from the actual resource and identifies sampled D32 via its depth views. The original smaller-declared-size counterexample is therefore rejected before submission. GraphTests checks both import forms and that Prepare is not called; ShadowDepthTests checks real D32 metadata and rejects both direct/deferred dimension mismatches. Existing regional content, pixel, cancellation and retention tests remain present. P2TargetedDebug.log material_rendering passed. |
| SC-01 missing per-handle asset failure | Closed | SceneInstance.cpp:225 first validates the full handle via Scene.Find, then returns its associated load failure, with Bridge errors as fallback. Removed/stale generations cannot inherit another model's asset error. SceneInstanceTests checks equality with the failed asset error, empty healthy-model error, removal, slot reuse and stale-generation queries. P2TargetedDebug.log scene_runtime_instance passed. |
| SC-02 destruction after dependency teardown | Closed | SceneInstance.cpp:79 checks Status.bClosed before accessing Tasks; repeated Close and destructor after successful Close return without using external dependencies. Open-scene close still enforces Main ownership and runs the existing cancel/join/detach sequence. CheckClosedDependencies closes the scene, destroys the whole dependency fixture, explicitly closes again, then destroys the scene. P2TargetedDebug.log scene_runtime_instance passed. |

## Direct impact review

No new actionable defect found within this repair scope. The new texture metadata interface exposes no native handles and does not introduce a Renderer dependency into RHI. Backend and test texture implementations were adapted; tracked mock texture lifetime behavior remains unchanged. Graph copy declarations retain compatible handles, consuming compilation still resets handles, and mutation rejection does not alter the frozen active graph. Scene documentation now states per-handle load errors and dependency-free repeated Close/destruction consistently with the implementation.

## Verification and limits

- Independently executed F:/HyperionEngine/out/build/debug/bin/graph_tests.exe successfully; output saved as P2ReviewerGraphDebug.log.
- Read P2TargetedDebug.log from F:/HyperionEngine/out/build/debug: render_graph, scene_runtime_instance, material_rendering, render_resources and rhi_backend_contracts all passed (5/5).
- Verified frozen snapshot hashes and snapshot SHA256.
- No old VS-tree binaries were used for this re-review.
- The reviewer did not repeat GPU tests already covered by the targeted log, full suites, Release builds, manual Viewer interaction, or OOM/device-loss testing. Release and broader style/boundary validation are being performed by the main agent; their final results remain separate delivery evidence.
- The previously measured performance regression is not resolved by closing these four P2 correctness findings and is intentionally outside this re-review.

## Final naming-only delta

After the frozen P2 re-review, Source/Tests/Renderer/ResourceTests.cpp renamed the abstract mock FTestTexture to ITestTexture and updated its TTrackedResource instantiation. Static inspection and reversing only this identifier replacement reproduce the prior snapshot file hash; no runtime logic changed. Final file SHA256: d884c3db836314f948d0c607f360ba88bc3b4420a1c7ede0ec9892ca781b0182. All other snapshot files still matched when checked. The four finding closures remain unchanged.

Reviewed final evidence: P2NamingFinalDelta.log passes semantic naming for the affected translation unit; P2FinalStyle.log passes source paths/casing and formatting; P2ReleaseTests.log reports 49/49 passing. No tests were repeated for this naming-only delta. The main agent is separately completing the final Debug full-suite run.
