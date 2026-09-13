# Skybox / skylighting quality audit

Audit date: 2026-09-13. Baseline `a70e9a8f2702ff02f4ce2040d2ff31479cbb46fa`; target was the uncommitted sky implementation, including tracked edits and new files. No unrelated existing changes were identified; staging remained empty and no commit was created during the audit. After audit closure, the user requested OpenSpec archive and Git commit on 2026-09-13.

The independent reviewer was started without inherited conversation context. Its initial scope comprised the 113 files in `out/SkyAudit/InitialSnapshot.json` (manifest SHA-256 `E85092BD235F877314E6F2632F06A65AD1EFA5DDD1182CBC5DB6D9B6EA93DA1E`). The coordinator did not modify those files during initial review. After the interrupted session restarted, both coordinator and reviewer independently verified that all 113 hashes still matched. The original interrupted reviewer had not completed startup or supplied a report.

## Findings and coordinator verification

All three findings are confirmed P2 defects introduced by this implementation and are within the requested behavior. The coordinator independently read each relevant call chain and reproduced the reported behavior before making local fixes. Detailed initial review: `out/SkyAudit/InitialReview.md`.

| ID | Trigger and verified failure | Repair |
| --- | --- | --- |
| SKY-A01 | Ready Sky → ConstantColor → same Sky before the next Tick cleared prepared data while retaining the completed load. `StateProbeInitial.log` reports `ready=1 data=0 status=Ready`. | `SetEnvironmentLight` retires the old load immediately on source changes. Returning to Sky starts preparation again; Ready cannot refer to cleared data. The new regression accepts pending preparation, awaits completion, checks the resolved asset identity and compares the rendered result. |
| SKY-A02 | After a missing file was supplied, applying the same failed path left `pending=0 failed=1` for 250 ticks, although direct AssetService loading succeeded. Reproduced by reviewer and coordinator in `SkyRetryProbe.log` and `MainRetryReproduction.log`; re-review also exposed cached wrong-type roots. | Explicitly setting a failed environment retires its failed load and allows a new attempt. Unpinned path requests invalidate the root cache before reading current bytes, including corrected wrong-type files. Existing complete sky data and requested-reference persistence remain intact during retry; pinned dependencies retain the shared cache. |
| SKY-A03 | Pure camera translation altered a nonuniform infinite sky because inverse VP construction and shader eye subtraction cancelled large float values. At X=100000, maximum image delta was 0.219608; at X=1000000 it was 0.517647. Both runs reproduced this in `SkyCameraProbe.log` and `MainCameraReproduction.log`. | The sky pass constructs projection × camera rotation without translation before inversion. The shader reads a direction directly, without an absolute eye-position subtraction. |

Production repairs are limited to `SceneInstanceNodes.cpp`, `SceneInstanceSky.cpp`, `ScenePipelineSky.cpp` and `Common/Sky.hlsl`. Regression coverage extends the existing `SkyRenderTests.cpp`; usage documentation describes explicit retry and direction reconstruction. No scene serialization, RHI lifetime protocol or module ownership was redesigned.

## Fresh validation

Raw outputs are under `out/SkyAudit`.

| Check | Result / log |
| --- | --- |
| Existing Debug complete suite before fixes | 66/66 passed, 344.91 s, `DebugInitialTests.log` |
| Debug / Release builds after all fixes | Both passed, `DebugFinalBuild.log`, `ReleaseFinalBuild.log` |
| Final Debug affected suites | 10/10 passed, 47.37 s, `DebugFinalTests.log` |
| Final Release affected suites | 9/9 passed, 10.96 s, `ReleaseFinalTests.log`; Release omits Debug-only dispatch-failure injection |
| Owned filenames and formatting | 499 source files passed, `StyleFinal.log` |
| Module boundaries | 470 source files / 29 modules passed, `Boundaries.log` |
| Naming of audit-edited C++ | Initial three units and final two units passed, `Naming.log`, `NamingFinal.log` (four distinct C++ units) |
| Whitespace / OpenSpec | `git diff --check` and strict change validation passed, `DiffCheckFinal.log`, `OpenSpecFinal.log` |

The affected suites cover scene management/runtime, dispatch failure (Debug), SceneViewer controls, environment preprocessing, sky rendering, shader compilation/reflection, shared render resources, scene rendering and sky GUI controls. The sky test checks zero D3D12 validation errors per rendered frame. Its new direction-gradient cube comparisons cover standard/reversed-Z, Forward/Deferred, an offset viewport with a nondefault depth range, two orientations including roll, and translations up to one million units. Every new translation image comparison has maximum delta **0** in both build configurations. Same-tick and separate-tick source changes, failed-file repair/retry, and old-generation retention also pass.

## Re-review

The first fixed snapshot contained 114 files; `FixedSnapshot.json` SHA-256 is `EC098ABBE113D6962F3E4B87AB0815FF2AA1D38D3DC5457C5BA70F6A1E11D8F1`. The original reviewer closed A01 and A03 after direct sky-test execution and rerunning the original camera probe (all four distances had maximum and mean delta 0). Its original missing-file retry probe also passed. CTest could not create its existing temporary log under the reviewer's sandbox, so it ran the built sky test directly from `out/SkyAudit`; this passed. Raw evidence: `ReviewerSkyFixedTests.log`, `ReviewerSkyFixedDirect.log`, `SkyRetryProbeFixed.log`, `SkyCameraProbeFixed.log`, `FinalReview.md`.

That re-review found an A02 residual: if a root file had successfully decoded as another asset type, correcting it and applying the same path still reused the successful old AssetService cache entry. Both reviewer and coordinator reproduced the mismatch; a fresh service loaded the corrected sky, and explicit root invalidation recovered the original service (`SkyWrongTypeRetryProbe.log`, `MainWrongTypeReproduction.log`). The follow-up repair invalidates the root cache on worker admission of an unpinned path selection. Pinned references keep their existing identity checks; dependent immutable textures retain the shared cache. The existing failed-retry regression now covers missing and wrong-type files; both cases pass in the final Debug/Release logs listed above.

The original reviewer independently checked all 114 paths in `FinalCandidateSnapshot.json` (SHA-256 `03031C23AEC4CE7168DCAEA559C32532A84CB6A1B0F8EDCFD3F330C1BC570AC8`) and closed **A01, A02 and A03**, with no new confirmed finding. It rebuilt and ran the wrong-type probe: the repaired path reached `ready=1 failed=0 status=Ready` without the later diagnostic manual invalidation. It also directly ran the final sky-rendering executable successfully, including both missing/wrong-type cases and prior regressions. Evidence: `BuildWrongTypeRetryProbeFinal.log`, `SkyWrongTypeRetryProbeFinal.log`, `ReviewerSkyFinalDirect.log`, `ClosureReview.md`.

After independent closure, only this audit record was updated to include the final disposition. Production and test files remain byte-identical to the reviewed final candidate. `out/SkyAudit/DeliverySnapshot.json` records the final delivered workspace hashes.

## Scope and limits

The initial review covered native HDR import and preprocessing, texture migration and cube storage, Materials/reflection/RHI/D3D12 bindings, Scene references and Save As, asynchronous publication/cancellation/close, renderer pass ordering and resource ownership, common IBL shaders, GUI selection, shipped content/provenance and affected tests. No other confirmed finding was reported.

The final full Debug/Release suites were not both rerun after these local fixes; the complete pre-fix Debug run and the post-fix affected suites are distinguished above. The initial implementation's complete Release run, Sponza screenshots/reloads and benchmarks remain earlier evidence in [verification.md](verification.md), not fresh audit measurements. This audit did not add large-image fuzz/stress, forced GPU allocation failures or a new performance benchmark. Global IBL limits in [SkyLighting.md](../../../../docs/SkyLighting.md) remain unchanged.
