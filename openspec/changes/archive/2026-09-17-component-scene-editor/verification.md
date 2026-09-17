# Verification

Evidence date: 2026-09-16. Baseline engine `eb40123683ef18ef8e3e3bed4cfa3ff63292a1d4`; baseline content `05f5382c2cc59d0fc90728457727e4fbce331e23`. Candidate changes remain uncommitted in both repositories.

The original component delivery evidence below refers to its recorded source fingerprint. Subsequent Transform display and whole-model reference follow-ups are recorded separately at the end; historical measurements are not claims about later binaries.

## Builds and checks

- Debug and Release full builds passed: `out/ComponentEditor/FinalBuildDebug.log`, `FinalBuildRelease.log`.
- Release full CTest: 73/73 passed, `FullTestsRelease.log`.
- Initial Debug full CTest: 71/74 passed, `FullTestsDebug.log`. Two integration benchmarks sampled before expanded scenes were ready; another expected two whole-model instances instead of eight primitive objects. The fixtures now allow 1,200 warmup frames and count expanded instances, while preserving original sample counts, exact pixel comparisons, draw/cache assertions and failure isolation. Affected Debug rerun: 9/9 passed, including all three previously failing tests, scene components, sky editing, Editor and fixtures; `RetestDebug.log`. Together these runs cover all 74 Debug tests successfully.
- Formatting and filename/include casing: 567 owned files passed, `Style.log`.
- Semantic C++ naming: all 67 changed/added translation units passed, `Naming.log`.
- Boundaries: 534 source files across 31 modules passed, `Boundaries.log`.
- `git diff --check` and `openspec validate component-scene-editor --strict` passed.

Final performance investigation identified excessive Debug iterator adoption in the per-property component lookup. `Slot<T>` now uses indexed vector access with normal Debug bounds checking; no compiler or STL safety setting changed. Full Debug/Release rebuilds passed (`LookupFullBuildDebug.log`, `LookupFullBuildRelease.log`); affected tests passed 9/9 in each configuration (`LookupTests-debug.log`, `LookupTests-release.log`). All 67 changed translation units passed semantic naming again (`NamingFinal.log`). Initial executables, identities and measurements are retained in `Candidate/Initial`; its collapsed/save samples overlapped compilation and are excluded from final conclusions. Final measurements ran after all build/check/test processes stopped.

## New behavioral coverage

`scene_management` covers composable capabilities, required/unique constraints, invalid/stale transaction rollback, multiple custom component instances, record round-trips, readonly property identities, presentation ranges, unknown-component save rejection, deterministic hierarchy/primitive expansion, shared geometry, inherited transforms and independent overrides. The custom-component round-trip also includes an instance whose ID equals its type ID.

`scene_runtime_instance` retains generation-safe loading/lifetime tests and verifies independent frozen viewport camera overrides. `sky_rendering` verifies Inspector-style sky rotation retains ready resources. Existing material/native publication tests verify dependency paths match component envelopes.

`editor_acceptance` exercises real GUI widget positions through Platform input: draft/edit/Apply, Render readback, Undo/Redo, save while later edits occur, reload, invalid/stale edits, independent viewport navigation and read-only mount save failure. The staged 111-object Sponza run is also preserved in `out/ComponentEditor/Parity/Editor.json`: `document_verified=true`, `document_dirty=false`, GPU validation errors 0, and 78 visible draws after independently hiding one of the original 79 visible primitives.

## Content migration

- Staged source reconstruction passed graph validation: 101 assets and 103 mesh instances.
- Frozen baseline Viewer and candidate Viewer rendered the same static camera at 1440x728, Deferred, reversed-Z, CSM, no UI/VSync: RGB pixels exactly equal; max channel difference 0. Evidence: `out/ComponentEditor/Parity/{Before,After}.png`, `Results.json`, CSVs and logs.
- Published scene schema 6 has 111 objects, including 103 independent mesh objects. Scene ID remains `0d2aba4bb12b5dd21a979d9f77d9c51b`; model ID remains `042854ff5b207627fe00afeb777ffc8b`.
- Published scene revision: `e243b280bc86b1002a1821ae15b42752ad41e43ca01325c381f32206b4e0bbfe`.
- Published model schema 3 revision: `d8bfca0607ded9a9d060d76474de661ad28fc2ac00c69d697ffbbd1ff5ba4f39`.
- Updated HyperionAssets root, import library, catalog and canonical source recipe; added one immutable model generation. Retained all prior dependency generations and shader text.
- Published graph validates; catalog contains 131 assets. Reimporting the recipe reports `Up to date`, 0 writes. Evidence: `PublishSponza.log`, `ValidatePublishedSponza.log`, `RebuildCatalog.log`, `ReimportPublishedRecipe.log`.

## Performance

The Chinese [comparison report](../../../../docs/ComponentEditorPerformance.md) and [portable data](../../../../docs/ComponentEditorPerformanceData.json) contain all eight Debug/Release, Editor/Viewer and static/moving comparisons. Each primary cell contains three independent processes and 900 pooled samples. The baseline and final candidate each have 24 runs; further evidence includes 12 collapsed-Outliner runs, six verified Save As runs, six no-UI runs and five adjacent frozen-baseline controls.

Release Editor static/moving means changed from 4.900/7.082 ms to 4.422/6.698 ms; Debug Editor from 12.077/27.557 ms to 12.213/28.270 ms. Debug moving Viewer changed from 24.628 to 24.953 ms after fixing the initial 41.7 ms regression. Main draw count (79), shadow draw count (404), descriptor count (273) and RHI allocation bytes remain unchanged. Release static/moving and Debug moving no-UI final frames are exactly equal between frozen baseline and candidate, with maximum RGB channel difference 0. The clean Editor preview has 111 objects, 79 draws, no document edits and no validation errors (`out/ComponentEditor/EditorFinal`).

Frozen baseline measurements are in `out/ComponentEditor/Baseline/Measurements`. Final candidate source fingerprint is `b7a2d748fd227b14c5e14c810b3c225f5c0537b891a418757b8e860ceadd5873`; identities and source hashes are in `out/ComponentEditor/Candidate`. Raw CSV/log/JSON evidence and hashes are packaged in `out/ComponentEditor/PerformanceEvidence.zip`. Generated `out/` evidence is local and excluded from version control. CPU/GPU clocks were not locked and run order was not randomized; small differences should not be attributed entirely to code.

## Final evidence and scope check

Rechecked all 616 source-manifest entries, four measured executable hashes, the migrated scene hash, 168 raw evidence hashes and ZIP integrity. All three final pixel comparisons passed. Links in all 20 changed Markdown files resolve. Final formatting/casing and dependency-boundary checks passed; no production code changed after the measured source fingerprint.

HyperionEngine contains the implementation, tests, portable documentation, benchmark tool and the completed active OpenSpec change. HyperionAssets changes are limited to `Scenes/Sponza.hasset`, `.asset-library.hasset`, `Catalog.hasset`, `Metadata/Sources.json` and one new immutable model generation. Both worktrees remain uncommitted; the change has not been archived.

## Scope limits

No ECS, scripting/GC, automatic reimport merge, geometry editing, general asset picker, object create/delete tools, Gizmo or Play mode. Transform display editing preserves affine data, including undisplayed shear. Runtime model instantiation and AssetTool import remain the geometry creation/replacement routes. Source fallback IDs are stable for a fixed source order, not across arbitrary upstream topology changes. Unknown component payloads are retained but saving is blocked until their registrations are available.

## Transform display follow-up

`FRecordDisplayLayout` projects a stored component into a reflected editing value and applies it to a detached candidate with both display and source validation. Transform owns its projection in Scene; generic Gui vector rows use field values and never depend on Scene component types. The authoritative affine matrix and persistence schemas are unchanged. Local Position/Rotation/Scale rows use colored, labeled X/Y/Z inputs; rotation is in degrees with right-handed fixed-axis X, then Y, then Z composition.

New regression coverage checks independent axis rotations and composition order; 500 deterministic affine cases including shear, reflection, singular scale and gimbal angles; exact no-op/parent-only/translation-only preservation; scene revision rejection; native round-trips; generic projection source/view validation, readonly values and registration identity. Real Editor input edits Position X, Rotation Z and Scale Y, then verifies Apply, exact Undo/Redo and save/reload. The initial Debug document run passed (`out/TransformInspector/Document.json`, `document_verified=true`, `document_dirty=false`, validation errors 0).

The source glTF was inspected directly: its single node has scale approximately 0.008 and references a mesh with 103 primitives. Their child transforms remain identity because the distinct vertex coordinates already place geometry in that shared source space. No source pivot, vertex data or HyperionAssets file is changed by this display follow-up.

Full Debug and Release builds passed (`out/TransformInspector/FinalBuildDebug.log`, `FinalBuildRelease.log`), followed by final Gui relinks in `GuiBuildDebug.log` / `GuiBuildRelease.log`. Related regressions passed 10/10 in each configuration (`TestsDebug.log`, `TestsRelease.log`); after the final generic vector field-access refinement, all four Gui/Editor tests passed in both configurations (`GuiTestsDebug.log`, `GuiTestsRelease.log`). These runs cover 11 distinct tests per configuration. Semantic naming passed for the 13 touched translation units plus the last Gui refinement; formatting/casing passed for 574 owned source files, and boundaries passed for 541 files across 31 modules. OpenSpec strict validation and whitespace checks passed.

The clean Release screenshot `out/TransformInspector/Preview.png` was visually inspected at the default 400-pixel side-panel width: Position/Rotation/Scale and all nine axis fields fit. Its report records 111 nodes, 79 draws, a clean document and zero validation errors. Sponza's SHA-256 still matches the component-stage candidate; all four retained historical executables match their recorded hashes. Documentation links resolve. The earlier performance report remains associated with its original source fingerprint and frozen programs; this display follow-up does not replace those samples with unmeasured claims. No commit or archive was performed.

## Whole-model reference follow-up (2026-09-17)

Scene publication no longer expands model nodes or primitives. Model-to-scene import creates one model object; native and source scene imports preserve authored topology. Scene importer revisions are 6 and publication records `scene_model_policy=whole-model-reference`, including when only the glTF importer is registered. No persistence schema downgrade is involved. Model and section material overrides remain instance-local, and the serialized `hyperion.staticmesh` identity stays unchanged while the Inspector/Outliner label is Model. Existing expanded scenes and the explicit CPU expansion helper remain supported; no automatic collapse or Editor split button was added.

`ModelReferenceTests.cpp` verifies multi-primitive compact publication, two instances with shared geometry and independent material/section overrides, JSON/native round trips, edited legacy children, and cache invalidation followed by zero-write reimport. Full Debug/Release builds passed, followed by publication-test-only rebuilds after fixture fixes. The relevant CTest selection passed 11/11 in each configuration, including actual Editor input, material GPU sharing and save/reload. Logs: `out/WholeModelReference/BuildDebug.log`, `BuildRelease.log`, `PublicationBuildDebug.log`, `PublicationBuildRelease.log`, `Tests-debug.log`, `Tests-release.log`. Style passed for 574 owned files, semantic naming for all seven changed C++ units, and boundaries for 541 files / 31 modules. OpenSpec strict and whitespace checks passed.

The guarded migration script `out/WholeModelReference/StageMigration.py` compared all 103 Sponza children and the source node with the shared model and recipe, including local transforms, names, enabled state, components and material state. Six deliberate destructive cases were rejected before staging. The staged and published scene exactly match the expected compact document; six camera/light objects and settings were preserved. The scene changed from 111 objects / 103 model components to 7 objects / one model component, with stable root `sponza-atrium` and asset ID `0d2aba4bb12b5dd21a979d9f77d9c51b`. Revision is `f299ef489d75e63d873ab4a45419786d89d86eb6d8636ae724761ca9d59344b6`; file size is 39,472 bytes instead of 205,098. All 123 existing immutable dependency files and the asset library index are byte-identical to their pre-follow-up versions. The model's schema-3 revision and original internal 0.008 scale remain unchanged.

HyperionAssets publication updated only `Scenes/Sponza.hasset`, `Catalog.hasset`, `Metadata/Sources.json` and the ignored cached scene recipe relative to this follow-up's input. Sponza dependency validation passed, catalog validation covered 132 assets including the catalog root, and repeat import wrote zero files. Backups, staged output, dependency hashes and commands are retained under `out/WholeModelReference`; previous iteration changes in both repositories remain uncommitted.

`ValidateStage.py` rendered the expanded and compact scenes using the same Release Viewer, fixed camera, 6,000 warmup plus 300 sample frames, and no UI; final RGB images are pixel-identical. The clean Editor preview was visually inspected: seven Outliner objects, one selected Sponza model, full TRS rows, 79 main draws and zero validation errors. Actual Editor document input verified TRS Apply, undo/redo, visibility, frozen async save, reload and dirty/save failure protections. See `StageImageParity.json`, `StagePreview.png`, `StagePreview.json`, `StageDocument.json`, and `PublishedCache.log`. Generated evidence is local and excluded from version control.

Final measurement completed 24 main runs (Debug/Release, Editor/Viewer, static/moving, three repetitions of 300 ready samples) plus 18 balanced-order static Editor controls across original, expanded/TRS and compact programs. See the final section of `docs/ComponentEditorPerformance.md` and `docs/WholeModelReferencePerformanceData.json`. The current main matrix is slower than historical samples; the unchanged original Editor is also slower in contemporary controls. Compact versus original static controls are 5.949 versus 5.901 ms in Release and 15.359 versus 15.355 ms in Debug. Compact GUI means are 0.171 / 0.931 ms versus expanded 0.217 / 1.589 ms. Release compact total time remains higher than expanded, principally in Render wait. No claim of universal speedup or absence of moving/Viewer regressions is made: those cases lack contemporary baseline pairs.

Final evidence verification passed for 627 source-manifest hashes, all four measured executables, the published scene, all 123 immutable dependency files, raw sample hashes, ZIP integrity and documentation links. Source fingerprint: `60eb40547a308370e44bed9430010dbb306a38d5dd7e94572b4e5b6568ed3001`. No production source changed after measurement. All 26 OpenSpec tasks are complete; no archive or Git commit was performed.


## Final combined quality audit and archive (2026-09-17)

The later [browsing-view change](../2026-09-17-separate-editor-view-and-scene-cameras/validation.md) supersedes the seven-object scene above with six objects and an InitialView preset. Historical benchmark samples remain tied to their recorded binaries and source fingerprints.

Three independent reviewers audited all 145 final uncommitted engine files and five asset files. Confirmed fixes cover component instance-ID collisions during round trips, focus events lost during asynchronous scene replacement, premature navigation-speed initialization, and browsing initialization blocked by partial resource failure. Calibration fixtures now author InitialView explicitly; whole-model count assertions and missing hyperion_check dependencies were corrected without weakening pixel or resource checks. The original reviewers re-reviewed each fix.

Final Debug and Release builds succeeded with zero warnings/errors in those build logs. The first repaired full Debug run passed 73/74; after the remaining partial-failure fix, all 11 affected tests including fixture dependencies passed. The final full Release suite passed 73/73. Formatting, semantic naming, module boundaries and diff checks passed. The 132-asset catalog graph validates; isolated Sponza reimport performs zero writes, and the formal asset repository remains byte-identical to the audited migration. Raw audit logs and snapshots are local under `out/QualityAudit20260917`.

The audit rechecked 261 historical evidence hashes and 16 result groups, but did not rerun a new baseline/candidate statistical performance comparison, TSAN or exhaustive fault injection. Both completed changes were archived and their requirements synchronized on 2026-09-17 before the authorized Git commits.
