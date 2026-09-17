# Validation

Implementation and migration verified on 2026-09-17. Following explicit user authorization, the four verified files were published to F:/HyperionAssets. No commit created.

## Code and behavior

- Visual Studio Debug and Release builds: Editor, SceneViewer, Scene tests, render scene-instance tests, publication tests and AssetTool. Release Viewer also built and rendered the staged scene.
- Focused CTest suites passed in both configurations: `scene_management`, `scene_runtime_instance`, `scene_viewer_controls`, `native_asset_publication`, and `editor_acceptance` (seven tests including required fixtures).
- Scene tests cover initial-view round trips, invalid lens/pose rejection, transactional settings, and schema-six camera/topology preservation.
- Renderer tests cover explicit strict preview, lens changes, disabled camera/parent, Camera-component removal, stale generations, optional runtime fallback and snapshot preservation.
- SceneViewer tests compare scene snapshots/revisions before and after navigation and verify camera-free browsing. Save/reload image comparisons use an explicitly authored initial view.
- Actual Editor widget input covers initial-view authoring, save-point undo/redo, camera creation, fresh generations, parent-relative world transforms, singular-parent rejection, applying the editor view, preview invalidation/clear, return and native save/reopen. Existing component edit, transform input, navigation and failed-save tests also pass.
- Full formatting/path checks passed (579 owned source files); boundaries passed (546 source files, 31 modules); semantic naming passed for 27 modified translation units. OpenSpec strict validation and `git diff --check` passed.
- The asset-import build reports three existing local-name shadowing warnings in `AssetPublicationGraph.cpp`; this camera change adds no warning and does not modify that unrelated file.

## Sponza migration

- Scene asset ID remains `0d2aba4bb12b5dd21a979d9f77d9c51b`; schema is 7. Published revision: `4d4421f7a4dca825b23da069442c9c68e7b8ace4492a0c23644d720a1ccce6a7`.
- Seven objects become six by removing only the verified placeholder `camera-main`. Its exact world matrix and lens become `initialView`; `defaultCamera` is empty. Remaining objects compare identically.
- Five unsafe migration cases are rejected: disabled camera, child reference, extra component, another scene selection reference and existing preset.
- All 123 immutable dependency files and `.asset-library.hasset` remain byte-identical. Staged catalog validates 132 native assets. Reimport is a cache hit with zero writes.
- Old runtime/current assets and new runtime/staged assets render identical 1440×728 pixels after loading, with one model group and 79 scene draws. Both runs report zero GPU validation errors. These captures verify parity; the runs are not a controlled performance benchmark.
- Debug and Release Editor view-workflow exercises both pass against the staged six-object scene.

Evidence is under `out/SceneBrowsingView`: `ImageParity.json`, `Before.png`, `After.png`, `ViewsDebug.json`, `ViewsRelease.json`, `TestsDebugFinal.log`, `TestsRelease.log`, `Naming.log`, `StageGraph.log`, and `StageCache.log`. `StageMigration.py` prepares the guarded staging copy; `PublishMigration.py` verifies the original files, backs up all destinations, atomically replaces individual files, validates the graph and rolls back on failure.

Publication destinations: `Scenes/Sponza.hasset`, `Catalog.hasset`, `Metadata/Sources.json`, and ignored `.cache/Sources/Scenes/Sponza.json` under HyperionAssets. No model, material, texture, shader, sky or other scene changes are included.

All four published files match the verified staging bytes. The production catalog validates 132 native assets; reimport from the production source cache reports zero writes. The 123 immutable dependencies and `.asset-library.hasset` still match their pre-migration hashes/bytes. Original destination files are backed up under `out/SceneBrowsingView/Rollback`. Production verification records: `PublishedVerification.json`, `PublishedGraph.log`, `PublishedCache.log`, and `PublishedScene.json` in the same evidence directory.

After publication, `editor_acceptance` passed against the production `/Game` mount in both Visual Studio configurations: Debug 49.27 seconds and Release 15.14 seconds. This includes component editing, viewport navigation, camera preview/return, history and scene save/reopen. Logs: `EditorDebugPublished.log` and `EditorReleasePublished.log`. OpenSpec strict validation passed and all nine implementation tasks are complete.


## Final quality audit and archive (2026-09-17)

The independent combined audit subsequently fixed navigation focus/speed initialization and partial-resource-failure browsing. View initialization now waits for published logical model data or a terminal model error when auto-framing, while an explicit InitialView can initialize immediately. Pending selected materials do not trigger premature auto-framing. Targeted tests cover both preset and no-preset failure cases, focus during asynchronous reload, retained movement speed, and unchanged authored scene revisions; the original strict partial-failure pixel comparison passes.

Final Debug/Release builds succeeded with zero warnings/errors in those build logs. Following a 73/74 full Debug run, the final affected 11/11 tests passed; the final full Release suite passed 73/73. Independent re-review found no unresolved actionable issue. Current AssetTool validates the 132-asset catalog with zero writes, and final hashes match all five audited asset files with no unexpected changes to the 142 baseline tracked files. Detailed combined evidence and limitations are recorded in the [component verification](../2026-09-17-component-scene-editor/verification.md#final-combined-quality-audit-and-archive-2026-09-17) and local `out/QualityAudit20260917` logs.

All nine tasks and all artifacts are complete. This change and its five requirements were archived and synchronized on 2026-09-17 before the authorized Git commits.
