# Scene camera/light follow-up investigation

Date: 2026-09-12. Working baseline: `dfe9e2501ff54d17ce03953f1dac1bbab3f90e01`.
Validation below was recorded before the follow-up commit. Archived OpenSpec evidence is unchanged.

## Repairs and regression coverage

- **Duplicate during loading:** duplication now preserves pending material selection provenance and independent whole-model/section edits. Geometry becoming ready cannot bypass pending material preparation. Full generation handles route completion after source removal and prevent a reused slot from receiving stale material results. Tests cover geometry-pending and material-pending copies, edits/clears, removal, failed material selection, and successful save/reload.
- **Large-coordinate view:** construct the view directly from the extracted orthonormal camera basis. Forming `Eye + unit Forward` lost direction at coordinates 33554432 and generated a nonfinite matrix. The regression failed before the repair and now passes.
- **Model-status work on camera/light edits:** separate model publication revision from scene metadata revision. Reuse model readiness/error results when model inputs are unchanged, while refreshing scene publication errors, active camera, and maintained node counts every tick. A 64-edit test verifies that camera/light-only mutations avoid model rescans; moving a camera with model descendants still invalidates the cache.
- **Intermittent shadow retirement assertion:** the test now explicitly gates Worker maintenance and drains earlier RHI work. It proves that captured GPU completion alone retains the old allocation and that releasing maintenance returns allocation to the original small baseline. The original baseline + 8 MiB bound remains; the assertion adds neither new rendering nor WaitIdle. Both depth conventions passed three runs each.
- **Dispatch allocation failure:** a separate Debug test executable injects ordinary-new failures at the two selected Dispatch shared_ptr control-block allocations. Four first-publication/update cases verify actual injection, retained publication token, retry completion, exact publication delta, and close. This does not claim coverage of every allocation site or the engine allocator.

## CRT popup

The reported `scene_dispatch_failure_tests.exe` popup came from the initial test injector selecting an MSVC Debug iterator-proxy allocation inside a noexcept constructor. Throwing there called terminate/abort before the publication failure handler could run. The revised injector identifies the intended Work/TaskState control blocks and excludes those proxy allocations. CRT reporting is directed to stderr inside this test process. No Windows runtime installation or system setting was changed.

The final adapter ran all four cases successfully: [DispatchAdapterTests2.log](../out/SceneOwnedCamerasAndLights/Followup/DispatchAdapterTests2.log).

## Completed functional validation

Debug targeted CTest passed 5/5: scene_runtime_instance, scene_dispatch_failure, scene_viewer_controls, cascaded_shadow_rendering, scene_rendering (50.71 seconds). Both Debug and Release builds completed after the model-status repair. The final isolated Dispatch adapter was rebuilt and passed again (3.40 seconds).

Evidence: [StatusTests.log](../out/SceneOwnedCamerasAndLights/Followup/StatusTests.log), [StatusBuild.log](../out/SceneOwnedCamerasAndLights/Followup/StatusBuild.log), [StatusReleaseBuild.log](../out/SceneOwnedCamerasAndLights/Followup/StatusReleaseBuild.log), [PrecisionBefore.log](../out/SceneOwnedCamerasAndLights/Followup/PrecisionBefore.log), [DispatchTests4.log](../out/SceneOwnedCamerasAndLights/Followup/DispatchTests4.log).

An independent reviewer examined the complete production/test changes and invalidation chains and reported no actionable findings. This was a source review with existing evidence, not a fresh full runtime suite: [FinalReview.md](../out/SceneOwnedCamerasAndLights/Followup/FinalReview.md).

## Image comparison: original maximum threshold still fails

All ten candidate runs exited successfully with zero D3D12 validation errors. Original thresholds remain maximum channel difference <= 2 and mean difference <= 0.1. Only the Debug/Release Model static pair passes both. Debug and Release measurements agree.

| Scene/motion (each Debug and Release) | Maximum | Mean | Pixels over 2 | Result |
| --- | ---: | ---: | ---: | --- |
| Showcase-static | 3 | 0.000022576 | 1 / 1048320 | Fail |
| Showcase-moving | 94 | 0.001417506 | 47 / 1048320 | Fail |
| Model-static | 2 | 0.000510031 | 0 / 1296000 | Pass |
| Sponza-static | 60 | 0.001208918 | 6 / 1048320 | Fail |
| Sponza-moving | 85 | 0.020400577 | 190 / 1048320 | Fail |

The old Viewer reconstructs an eye from float yaw/pitch/distance and uses LookAt with its orbit target. Scene navigation updates a float rigid transform and the renderer consumes its extracted basis. The 600-frame C++ legacy-orbit oracle measured maximum eye deviation 2.71127e-05 and forward deviation 1.11064e-06. At the maximum-difference pixels, baseline 3x3 neighborhood contrast is 122 for Showcase static, 94 for Showcase moving, 67 for Sponza static, and 90 for Sponza moving. These observations support a numerical explanation at high-contrast edges, but are not a matched-input rendering proof of the sole cause. [Pixel diagnostics](../out/SceneOwnedCamerasAndLights/Followup/PrecisionCaptures/DifferenceDiagnostics.json). The maximum-pixel failures remain unresolved; neither a looser threshold nor a second authoritative Viewer camera state was introduced. Compared with the preceding view implementation, the repair makes Model static pass, while Showcase static now exceeds the maximum by one at one pixel.

Capture settings retain the prior baseline protocol: forward rendering, CSM 1024, no VSync/UI, hidden window, 840 total / 240 warmup frames (Sponza 10840 / 10000), camera step 1 for moving captures. [Capture results and exact commands](../out/SceneOwnedCamerasAndLights/Followup/PrecisionCaptures/Results.json).

## Native GUI replay boundary

Native mouse replay could not start: the approved computer-use Node kernel failed during sandbox initialization with `helper_unknown_error: apply deny-read ACLs`. A reset and retry after the tool update produced the same result. No native UI input was sent. Passing controller tests does not establish the remaining real mouse interaction sequence.

## Performance and final checks

The original legacy checkout and candidate completed 16 configuration/pipeline/motion cells, three interleaved pairs per cell (96 runs). Each run used 840 frames with 240 warmup. Values below are percentage changes of the median of three per-run statistics. Review triggers retain the original > 5% mean and > 10% P95 limits. Three Release cells trigger review; all eight Debug cells remain below both limits. These measurements compare the complete candidate with the legacy baseline, so they do not isolate the model-status optimization alone.

| Configuration / pipeline / motion | Frame mean | Frame P95 | Pipeline prepare mean | Pipeline prepare P95 | Trigger |
| --- | ---: | ---: | ---: | ---: | --- |
| debug / forward / static | -6.02% | +3.71% | -5.21% | +4.33% | No |
| debug / forward / small-camera | -17.22% | -19.84% | -17.41% | -20.44% | No |
| debug / forward / large-camera | -0.89% | -0.05% | -2.09% | -1.20% | No |
| debug / forward / moving-light | -6.71% | -6.69% | -7.31% | -7.47% | No |
| debug / deferred / static | -1.33% | +1.91% | +1.62% | +4.16% | No |
| debug / deferred / small-camera | +2.68% | +7.11% | +0.97% | +7.08% | No |
| debug / deferred / large-camera | +1.49% | +1.38% | +0.27% | +1.03% | No |
| debug / deferred / moving-light | +1.08% | +4.09% | -0.14% | +3.72% | No |
| release / forward / static | -4.60% | +10.51% | -8.99% | -6.91% | Yes |
| release / forward / small-camera | -1.42% | -8.94% | -2.88% | -1.67% | No |
| release / forward / large-camera | -3.27% | +0.34% | -4.89% | -4.92% | No |
| release / forward / moving-light | -0.68% | +1.19% | +0.65% | -2.35% | No |
| release / deferred / static | +2.69% | -5.40% | +16.50% | +40.40% | Yes |
| release / deferred / small-camera | -0.88% | +2.00% | -12.47% | -19.41% | No |
| release / deferred / large-camera | +5.82% | +10.90% | +6.23% | +5.83% | Yes |
| release / deferred / moving-light | -5.82% | -10.51% | -5.53% | -6.11% | No |

All 97 runs (including the 3240-frame combined camera/light case) completed with zero validation errors and 79/79 models ready / 0 failed in their completion logs. Sample counts were exact, GPU submission sample identities unique, and candidate BVH rebuild/refit counters zero. The sustained run had 3000 measured samples, zero failed scene/shadow items, exactly 115 descriptor allocations and 5 created pipelines throughout. GPU allocation ranged from 35,876,864 to 43,741,184 bytes (first 35,876,864; last 43,741,184); this run alone does not prove a long-term plateau.

Evidence: [matrix comparisons](../out/SceneOwnedCamerasAndLights/Followup/PerformanceMatrix/Comparisons.json), [raw metrics and commands](../out/SceneOwnedCamerasAndLights/Followup/PerformanceMatrix/RawResults.json). No builds or other GPU checks ran alongside these measurements.

The three triggering Release cells completed a separate five-pair repeat, alternating which version runs first, with 3240 total / 240 warmup frames. All 30 runs passed the same readiness, validation, sample-identity and candidate BVH checks. None triggered the original review limits. The longer repeat did not reproduce a stable regression; it does not erase or convert the original 600-sample trigger records into passes.

| Release repeat / pipeline / motion | Frame mean | Frame P95 | Pipeline prepare mean | Pipeline prepare P95 |
| --- | ---: | ---: | ---: | ---: |
| forward / static | -4.68% | -6.10% | -5.31% | -4.68% |
| deferred / static | +1.78% | +1.27% | -0.65% | -2.85% |
| deferred / large-camera | -0.90% | -0.23% | -1.65% | -2.19% |

Evidence: [repeat comparisons](../out/SceneOwnedCamerasAndLights/Followup/PerformanceTriggerRepeat/Comparisons.json), [repeat raw metrics and commands](../out/SceneOwnedCamerasAndLights/Followup/PerformanceTriggerRepeat/RawResults.json).

Final static checks passed: 456 source paths/formats, 436 source files across 28 module boundaries, and semantic naming for all 11 modified/new C++ translation units. A temporary Windows-header probe outside Tests/Private/Adapters was rejected; removing it restored the passing boundary check. Release targeted CTest passed 4/4 (12.89 seconds). Ninja and regenerated VS projects both register the allocation-failure test in Debug and omit it in Release; the VS hyperion_check target depends on its executable. VS configuration was verified without claiming a new full VS build/test suite.

Evidence: [static checks](../out/SceneOwnedCamerasAndLights/Followup/FinalStaticChecks.log), [Release tests](../out/SceneOwnedCamerasAndLights/Followup/FinalReleaseTests.log), [CTest registration](../out/SceneOwnedCamerasAndLights/Followup/FinalCTestRegistration.json).

## Remaining work and repository state

The remaining cross-version maximum-pixel failures require a matched-camera-input rendering comparison to establish the residual cause; numerical proximity alone is insufficient to close that acceptance item. Native mouse replay remains blocked by tool initialization. Neither item is marked complete.

The independent reviewer also checked the report against its linked logs and found no factual corrections (before the final repeat table was added). No source changed after the source review. The final source hash snapshot records 17 modified/new source/tool files. At the validation checkpoint, HEAD was `dfe9e2501ff54d17ce03953f1dac1bbab3f90e01` and the index was empty. The subsequent user-requested commit includes the reviewed repairs and this report; no additional archive operation is required.
