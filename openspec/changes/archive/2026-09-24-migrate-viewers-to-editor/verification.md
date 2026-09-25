# Migration verification

## Workspace and scope

- Baseline HEAD: `1942ee6dc4456cf3a34bba1687c3a4db54e5bdd6`; the initial worktree was clean. No Git commit or push is part of this change.
- The pre-change CTest inventory contained 115 registrations; the targeted Editor/scene/Automation baseline passed 6/6.
- Retired application/plugin directories, composition options, experimental configurations, adapters, launch scripts and obsolete reports are removed. Shared Runtime algorithms and AssetTool/Automation remain.
- `coverage.md` maps the retired consumers to their replacement implementations and validation.
- Source, tools, current documentation and main specifications contain no references to the retired applications. Existing OpenSpec archives are historical evidence: 135 Markdown files across the prior archive retain old names. They are deliberately not rewritten, in accordance with `docs/CodingStyle.md` and the approved design.

## Build configurations

| Build | Triangle / DebugUI | RenderDoc | Tracy | Result |
| --- | --- | --- | --- | --- |
| `out/build/debug` | ON / ON | ON | OFF | Full build passed |
| `out/build/migration-release` | OFF / OFF | OFF | OFF | Full build passed |
| `out/build/profile` | ON / ON | OFF | ON | Full build passed |

The ordinary Release CLI was locked by existing MCP services. The complete optional-plugin-off configuration was therefore built in `migration-release`; those services were left running. Compiler/linker configuration and the full test suite are otherwise selected through the repository CMake setup.

## Completed checks

- Full Debug CTest: **104/104 passed**, 669.49 seconds; `out/MigrationFinal/DebugTests.log`, including real RenderDoc capture and Editor interaction cases.
- Full Release CTest: **95/95 passed**, 283.55 seconds; `out/MigrationFinal/ReleaseTests.log`. Disabling optional plugins no longer hides unrelated tests.
- Profiling contracts, controls and real Tracy acceptance: **3/3 passed**, 33.60 seconds; `out/MigrationCompletion/Tracy.log`. This includes two real collector connections, resumed/nested/exception task scopes, native GPU query cancellation/reuse, exactly 120 Editor frame scopes per bounded capture, detail/GPU categories and startup asset-loading scopes.
- Format/path checks cover 870 owned files. Semantic naming covers 574 translation units; five subsequently modified translation units were rechecked using Debug/profile compile settings. Evidence: `out/MigrationStyle.log`, `out/MigrationIncrementalNaming.log`, `out/MigrationFinalFormatting.log`.
- Dependency boundaries: 834 source files / 38 modules passed.
- Main OpenSpec specifications: **91/91 passed** strict validation after synchronization. Evidence: `out/MigrationSpecsValidation.log`.
- The initial full Debug run passed 101/104. The two depth-preview failures were fixed by retaining the original backbuffer default while allowing explicit offscreen output. Automation process lifetime was made test-controlled, and its float assertion now uses a tolerance. All three failed cases subsequently passed.
- Editor rendering acceptance covers Forward/Deferred and both GBuffer layouts, both startup depth conventions, persisted depth requests, optional contact-shadow absence, contact/HZB and CSM previews, culling/freeze/batching/bounds, completed screenshots and invalid-setting rollback without scene dirtiness.
- Structural tests cover shared resource identity, duplicate selection restoration, keep-children world transforms, recreated handle remapping, settings restoration and repeated undo/redo. Native model tests retain delayed IO, readiness, failure, material pixels and resize coverage.
- Real Tracy execution exposed an existing native test assertion that read query ownership from the parent batch instead of its recorded child pass. The test now follows the current representation and retains every cancellation, retirement, bounded-pool and recovery assertion; no backend production behavior was changed.

## Workload checks

The script runs validate coverage and repeatability; they are not evidence of a performance improvement over the retired application. Loading, Editor GUI, viewport extent, selection outlines and actual GPU submission IDs are explicitly accounted for.

- `MeasureShadows.py`: static, camera, light and combined motion, shadows off/on, plus a sustained resource check passed. The light workload commits one shared document transaction and restores authored state before exit. Evidence: `out/MigrationTools2/Shadows/Summary.json`.
- `InstanceBenchmark.py`: ordinary/instanced draws for static and moving inputs passed; `out/MigrationTools2/Instances`.
- `CpuSubmissionBenchmark.py`: 1 and 100 item static/moving fixtures passed; `out/MigrationTools2/Submission`.
- `BatchPlannerComparison.py`: identical binaries, static/small/large movement, matched viewport image hashes and draw coverage passed; `out/MigrationFinal/Comparison/Summary.json`. Benchmark navigation uses the shared Orbit controller, independently of interactive Editor Fly navigation.
- The shared settings generator emits reflected floating-point values. Single-model workload import explicitly supplies the selected Game root and references `/Game/Models/Showcase.hasset`, retaining native model/material identities instead of copying mounted assets. Each matrix reuses its prepared workload. Repeated import into the same output directory passed without creating a duplicate Models directory.

## Final verification

- Final incremental builds passed in all three configurations. After the exposure lower boundary was aligned at 0.05, Debug affected tests passed **4/4** and Release affected tests passed **3/3**, including render controls, real Editor rendering and D3D12 recovery; Debug also reran Automation capability parity. Evidence: `out/MigrationCompletion/DebugAffected.log`, `ReleaseAffected.log`.
- Forward/Deferred workload matrix passed **48 runs**: Scene/Model, static/moving camera, shadows off/on, Forward/Deferred compact/Deferred high, two repetitions with reversed order, 320x240 viewport, 60 ready warmup frames and 40 samples per run. Exact GPU submission matching, equivalent per-frame workloads, nonempty coverage, zero validation errors and bounded resources all passed. Evidence: `out/MigrationCompletion/DeferredFinal/Summary.json`.
- Sponza Editor workload passed **4 runs**: Debug/Release with static/moving camera. Evidence: `out/MigrationCompletion/Components`.
- Final Release shadow matrix passed **9 runs**, covering static/camera/light/combined motion with shadows off/on, plus **1500 sustained samples** after warmup. PSOs/descriptors stayed stable and GPU memory stayed bounded. Evidence: `out/MigrationCompletion/Shadows/Summary.json`.
- All 36 Python files under tools and Source/Tests parsed successfully. The two benchmark helper changes were exercised by the full 48-run matrix and repeated same-directory imports.
- Final source/tool/current-document/main-spec scans found no retired application references. `git diff --check` passed; the independent `F:/HyperionAssets` worktree remains clean. HEAD remains the baseline commit; no commit or push was performed.
- Main specifications were synchronized before archival: all added/modified requirement bodies match the deltas, and all removed requirements are absent. Existing unrelated changes and archives remain untouched.
- OpenSpec archived this change as `2026-09-24-migrate-viewers-to-editor` with all tasks complete. `--skip-specs` avoided reapplying the already synchronized deltas. The archive command emitted non-blocking proposal warnings about requirement/scenario formatting and the number of deltas; explicit strict change validation passed before archive, and strict main-spec validation passed **91/91** again after archive. The only remaining active change is the unrelated `simplify-reflection-declarations`.
