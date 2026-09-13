# Clustered local lighting verification

## Scope and environment

Windows, D3D12, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled, Tracy disabled. The implementation remains uncommitted; baseline commit is `112eaeb32f0df422022bb0ef4be19dd9e89b7ee1`. No scene light, camera or model asset was retuned. The only change to `experiments/Scene.json` is explicit default enablement of `clustered_lighting`.

## Rendering acceptance

Release Viewer captures use the unchanged `experiments/Scene.json` at 1440x728, exposure 2.5 and reversed Z. Each accepted capture reports all 79 Sponza model draws, three visible point lights where applicable and zero GPU validation errors. Clustered captures have zero local volume draws. Forward opt-out has zero active local lights.

| RGB image comparison, 8-bit output | Maximum channel difference | Mean channel difference |
|---|---:|---:|
| Previous committed appearance vs current Deferred volumes | 0 | 0 |
| Previous committed appearance vs default Deferred clusters | 1 | 0.004098 |
| Default Deferred clusters vs clustered Forward | 17 | 0.374601 |
| Clustered Forward vs Forward opt-out | 231 | 15.134823 |

The first two comparisons establish preservation of the existing point-light appearance. Deferred and Forward have different GBuffer precision and shading derivative paths, so their full-scene difference is reported separately. Equivalent controlled receivers are compared with tighter GPU regression tolerances. Forward opt-out removes the authored point-light pools; it is not a parity reference for enabled lighting.

Images and raw results (ignored local outputs): [default clusters](../../../../out/ClusterLighting/Cluster.png), [legacy volumes](../../../../out/ClusterLighting/Volumes.png), [Forward clusters](../../../../out/ClusterLighting/Forward.png), [Forward opt-out](../../../../out/ClusterLighting/ForwardOff.png), [previous appearance](../../../../out/ClusterLighting/Previous.png), [image metrics](../../../../out/ClusterLighting/Images.json).

Capture command, with `--no-clustered-lighting` for volumes or `--pipeline forward` for Forward:

```powershell
out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json --hidden --frames 1200 --capture out/ClusterLighting/Cluster.png --verify-model
```

## Regression coverage

- CPU: 6,000 sampled receivers per depth convention, conservative lists, unique ordered indices, compact offset bounds, nontrivial camera/sub-viewport, assignment reuse, radiance-only attribute updates, old generation retention, empty buffers and 128 lights per cell without truncation.
- GPU: standard/reversed Z; fused directional/cluster, cluster-only and Forward paths; analytic attenuation/BRDF, point/spot overlap, cone falloff, camera inside/outside, near/far and light-center cases, sub-viewport reconstruction, dense 81-light accumulation, unlit and lit transparency, culling modes, toggles, resize and queued scene/light changes.
- Resource lifetime: stationary frames reuse descriptors/PSOs, moving lights keep live cache objects and GPU memory bounded, and 32 independent textured materials with CSM repeatedly toggle Forward clusters under the default descriptor capacities.
- Materials/config/shaders: persisted default-true toggle and opt-out, builtin PBR semantic declarations and disabled buffers, native importer revision migration, structured buffer reflection stride and dedicated no-directional permutation.

## Issues found and resolved during acceptance

1. Sponza Forward initially failed with `Material descriptor capacity exceeded ... heap=1 requested=6 capacity=512`. Static material preparation eagerly created descriptor sets for unused passes with default View resources, keeping them beside the actual shadow/cluster sets. It now uploads sources while deferring descriptor-table publication to actual draws. This does not increase capacities, clear global caches or introduce GPU waits. The 32-material regression and native Sponza exercise the fix.
2. `--verify-model` could accept a bright clear background even when scene draw receipts reported failure. It now checks draw errors, scene readiness and nonzero submitted model draws.
3. An initial moving-light test incorrectly treated cumulative `DescriptorAllocations` as a live allocation gauge. Changed immutable buffers legitimately require new tables. The test now checks live cache objects and GPU allocation bounds while retaining strict stationary reuse checks. Motion benchmarks report cumulative counts separately.
4. A first no-VSync benchmark with only 1,200 warmup frames was rejected because native scene loading had not completed. It is excluded. Accepted runs use 6,000 warmup frames and assert ready/nonempty geometry and zero failed items throughout all 1,000 measured frames.
5. Small camera motion recomputed byte-identical lists but initially uploaded new buffers. Exact list comparison now preserves unchanged sources. Fullscreen resource owners also survive numeric-only camera changes; constant snapshots remain independently scoped. Dedicated CPU and GPU regressions check both reuse paths. The measurements below include these changes.

## Performance

Serial Release runs, VSync off, default CSM, 6,000 warmup frames plus 1,000 measured frames. Motion uses the Viewer's existing small repeating camera orbit. Every measured frame has 79 model draws, zero failed model/shadow items and valid completed-submission GPU timings. Frame time includes application/CPU scheduling; GPU columns sum instrumented render passes, not presentation. These are single-run observations, not a broad performance claim.

| Path | Camera | Mean frame ms | Mean GPU pass ms | Mean cluster preparation ms |
|---|---|---:|---:|---:|
| Deferred clusters | Stationary | 4.669338 | 0.260036 | 0.004694 |
| Deferred volumes | Stationary | 4.662192 | 0.281796 | 0 |
| Deferred clusters | Moving | 5.907429 | 0.261315 | 0.027566 |
| Deferred volumes | Moving | 5.887994 | 0.279907 | 0 |
| Forward clusters | Stationary | 4.236997 | 0.307013 | 0.005181 |
| Forward clusters | Moving | 5.987920 | 0.314061 | 0.027953 |

The small three-light scene has essentially equal Deferred frame times between algorithms. Its grid has 6,624 cells, 5,085 occupied cells, 6,423 references and 78,876 bytes of cluster buffers. All six measured runs retain stable descriptor allocation and pipeline creation counters. Static GPU allocation is constant; moving allocation range is 128 KiB in Forward and 192 KiB in Deferred. This camera path recomputes assignments but preserves list contents; separate moving-light GPU regressions exercise changed sources and retirement.

Before list/source ownership reuse, measured moving frame means were 8.998046 ms in clustered Forward and 7.252723 ms in clustered Deferred. Those intermediate runs are retained as `BeforeReuseForwardMoving.csv` and `BeforeReuseDeferredMoving.csv` for diagnosis, not final acceptance.

Raw CSV/logs are in `out/ClusterLighting/{Forward,Deferred,Volumes}{Static,Moving}.*`; checked aggregates are [Benchmarks.json](../../../../out/ClusterLighting/Benchmarks.json). A representative command is:

```powershell
out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json --hidden --no-vsync --frames 7000 --benchmark-warmup 6000 --benchmark out/ClusterLighting/DeferredMoving.csv --benchmark-camera
```

## Final checks

- Full Debug and Release builds passed, including automatic native sample asset regeneration (`DeliveryDebugBuild.log`, `DeliveryReleaseBuild.log`).
- Release full CTest suite: **62/62 passed**, 145.56 seconds (`FullRelease.log`).
- Final Debug affected-suite regression including material/resource, shaders, spatial/Deferred, model and SceneViewer acceptance: **15/15 passed**, 132.49 seconds (`DeliveryDebugTests.log`). Fixtures are included in this count. An earlier complete Debug suite also passed 63/63 before acceptance-driven resource refinements (`FullDebug.log`).
- Final default Sponza capture after those tests: 79 model draws, three visible points, zero local-volume draws, clustering enabled, GPU validation errors zero. Previous-image maximum RGB error remains one 8-bit code value.
- `CheckStyle.py`: 478 owned source files and formatting passed. Semantic naming/local declarations passed for all 28 changed/new C++ translation units.
- `CheckBoundaries.py`: 451 files across 28 modules passed.
- `openspec validate add-clustered-local-lighting --strict` and `git diff --check` passed.
- Git HEAD remains the baseline commit, index is empty, no commit or archive was performed.

All command logs and image/benchmark aggregates are under `out/ClusterLighting`. The checked-in implementation explanation is [docs/ClusteredLighting.md](../../../../docs/ClusteredLighting.md).

## Practical limits

This implementation builds conservative projected-AABB cell ranges on CPU and reads immutable upload-heap buffers. Cell lists can contain false positives, rejected by exact per-light attenuation. It has no compute builder, opaque-depth occupancy optimization, normal rejection or local-light shadows. Budgets are 65,536 candidate lights, 1,048,576 cells and 16,777,216 references; overflow reports an error without silent light truncation. Sponza has three lights, so its timings do not establish scalability to the maximum budget.

## Independent quality audit (2026-09-13)

The audit covered all tracked and new files in the uncommitted clustered-lighting change against `112eaeb32f0df422022bb0ef4be19dd9e89b7ee1`. An independent reviewer started without conversation history, read the requirements and applicable repository rules, and traced scene publication, visibility queries, cluster assignment, material/fullscreen ownership and shader consumption. The review also covered both depth conventions, viewport offsets, clipping boundaries, capacity failures, all lighting routes and opt-out behavior, importer migration, configuration and regression-test changes. No confirmed defect requiring a repair was found. No implementation code was changed during this audit.

The reviewer independently ran Debug `scene_spatial_visibility` and `deferred_rendering`: **2/2 passed**, 26.10 seconds, and checked `git diff --check`. Full builds, the Release suite, Sponza image comparisons and performance measurements above were inspected as existing evidence rather than rerun during the audit.

The primary agent separately traced resource-cache ownership and ran an out-only native GPU probe derived from `DeferredRenderTests.cpp`. It rendered 32 independent normal-textured materials with Forward clustering and CSM while changing point-light intensity and position every frame. Standard and reversed Z each completed 120 frames; every frame submitted all 32 model draws and reported zero GPU validation errors. After 20 warmup frames, live material-cache objects ranged from 121 to 154 (standard Z) and 122 to 155 (reversed Z), ending at 121 and 122 respectively. GPU allocation ranged from 23,986,176 to 24,117,248 bytes in each run. Cumulative descriptor allocations increased as expected for changed immutable sources, while live objects and GPU memory remained bounded during this probe. This supplements the checked-in Forward toggle test with continuous new buffer generations; it is not a long-duration stress or maximum-light-count test.

Reproducible probe generator, isolated executable and raw logs remain in ignored `out/ClusterLightingAudit/{BuildProbe.py,DynamicForwardProbe.cpp,Probe.exe,Build.log,DynamicForward.log}`. The probe did not replace the normal test binary or modify owned sources. The audit leaves the implementation uncommitted and the index empty.
