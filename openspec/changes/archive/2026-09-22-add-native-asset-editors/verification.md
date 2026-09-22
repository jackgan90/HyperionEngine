# Native asset editor verification

## Delivered behavior

- Content Browser opens native Texture, Model, Sky and Material documents in separate tabs beside the existing scene document. Invalid/unsupported assets produce diagnostics. Shader files remain text and are excluded.
- Each asset owns a detached draft, save baseline and interaction-based history. Save completion records the submitted state; edits made during save remain dirty. Same-ID/revision preconditions reject stale or removed files.
- Texture inspection includes mip, face, channel, exposure, pixel values and pan/zoom. RGBA8 2D encoding changes preserve mip0 and rebuild lower mips as one history operation.
- Model editing supports names, affine-preserving local PRS, existing material slots and references. Material editing uses declared parameter types and the real shader, with explicit/default values and reset. Sky previews use native lighting products and reference spheres.
- Successful saves refresh affected scene resources and open previews without discarding authored scene edits or history. Scene refresh prepares replacement GPU resources before publication and retains existing resources on preparation failure.
- Persistent Catalog reflection/API and AssetTool creation were removed. Discovery still supplies the in-memory UUID index and reference resolution.

The first-version field matrix and user controls are documented in `docs/Editor.md`; native persistence/index contracts are in `docs/NativeAssets.md`.

## Initial implementation checks

All commands run from the repository root on Windows / D3D12.

| Check | Result | Local evidence |
| --- | --- | --- |
| Full Debug build, `./tools/Build.ps1` | Passed | `out/AssetEditorBuildFinal.log` |
| Full Release build, `./tools/Build.ps1 -Preset release` | Passed | `out/AssetEditorReleaseBuildFinal.log` |
| Debug CPU suite, `ctest --test-dir out/build/debug -LE 'gpu\|desktop' --output-on-failure -j 6` | 51/51 passed | `out/AssetEditorCpuTestsFinal.log` |
| Debug affected Editor/rendering/lifecycle suite | 22/22 passed, including 15 GPU/desktop tests | `out/AssetEditorRegression.log` |
| Release CPU and targeted desktop suites | 50/50 CPU and 7/7 targeted checks passed | `out/AssetEditorReleaseCpu.log`, `out/AssetEditorReleaseDesktop.log` |
| Final Debug scene/docking regression | 2/2 passed | `out/AssetEditorDesktopFinal.log` |
| Repeated asset document/desktop acceptance with fresh fixtures | Three consecutive passes per configuration, 2/2 checks per run | `out/AssetEditor-{debug,release}-Repeat{1,2,3}.log` |
| Formatting and filename/include casing | Passed, 736 owned files | `python tools/CheckStyle.py` |
| Semantic C++ naming and boolean declarations | Passed, 483 translation units; final cleanup files rechecked | `out/AssetEditorStyle.log` |
| Module boundaries | Passed, 700 C++ files / 35 modules | `python tools/CheckBoundaries.py` |
| OpenSpec | Change validation passed; all 80 changes/specs passed strict validation | `out/AssetEditorOpenSpec.log` |
| Diff whitespace | Passed | `git diff --check` |

The Debug affected-suite expression was:

```powershell
ctest --test-dir out/build/debug -R '^(editor_|shared_asset_rendering|scene_runtime_instance|scene_rendering|gui_texture_rendering|model_rendering|lifecycle_recovery|plugin_applications|native_publication_cli)' --output-on-failure
```

## Focused coverage

`editor_asset_documents` covers independent drafts, interaction merging/cancel, undo/redo, editing during asynchronous save, stale-baseline rejection, removed-file rejection, exact texture mip undo/redo, model PRS save/reload with retained shear and stable IDs, and rejection of the removed Catalog type.

`editor_asset_editors` exercises actual Content Browser double-clicks and GUI/key input for Texture, Model, Sky, PBR Material and custom Shader Material. It checks name edit/save/undo/redo; a numeric material edit; texture and model-slot reference replacement; texture encoding and mip0 preservation; floating cube preview; custom vector override/save/reset/undo/redo; tab-close cancel and save/close; and application-close cancellation. Scene assertions retain unsaved edits, local/reference overrides, shared geometry and query data, and scene undo/redo after asset saves.

Native asset/registry/publication regressions cover discovery, duplicate IDs, relocated references, serialized saves and rejection of the removed CLI `catalog` command. Existing Editor tests cover scene editing, camera views, selection, gizmos, placement, capture, root switching and startup. Rendering/lifecycle tests cover shared resources, scene/model drawing and GUI texture composition.

Captured Editor images are under `out/build/debug/AssetEditor-{Texture,Model,Sky,Material,CustomMaterial,Cube}.png` and the corresponding Release build directory. The Debug images were visually inspected for actual asset rendering and the editable/read-only property presentation.

Repeated testing found that GUI assumed a tab was closed before the owner completed its unsaved-document prompt. The wrapper now defers removal to the owner, retaining tab order after Cancel. Acceptance also explicitly hovers the overlapping close icon before pressing it and waits for tab scrolling to settle. Both configurations passed three fresh-fixture runs after these corrections.

## Follow-up: tab close and panel movement

The user reported that closing a non-scene tab exited the Editor and that moving the floating Place Object panel was interrupted while asset tabs were active. Both have regression coverage in `EditorAssetWorkspaceAcceptance.cpp`, using the existing Editor input path.

- Closing a clean rendered tab reproduced `GUI draw references an unregistered texture` in `out/AssetWorkspaceRegressionBaseline.log`. The GUI had already emitted preview draw commands when the document and texture binding were removed. Accepted clean/discard closes now execute at the next Main update, after the preceding frame has rendered and before new GUI submission.
- Restoring the original placement cancellation reproduces interrupted floating-panel movement in `out/AssetWorkspaceFloatingBaseline.log`. Docking/moving windows also uses a GUI drag payload. `CancelPlacement` now cancels only the placement payload, preserving window gestures; global focus/Escape/root-transition cancellation remains unchanged.
- Acceptance directly closes clean Model, Sky, Material/custom Material and 2D/cube Texture previews, including the last tab; discards a dirty material and verifies its saved name on reopen; and holds/drags/releases the floating Place Object panel with three asset tabs open while checking scene history preservation. Existing dirty-close Cancel/Save-and-Close coverage remains active.

The panel test explicitly loads a floating layout matching the reported interaction. It waits for the requested active preview, establishes hover before pressing, and allows the normal docking transition after release before checking that further pointer motion cannot move the panel.

| Follow-up check | Result | Local evidence |
| --- | --- | --- |
| Debug and Release Editor builds | Passed | `out/AssetWorkspaceFinal{Debug,Release}Build.log` |
| Debug asset document/input acceptance | 2/2 passed | `out/AssetWorkspaceFinalDebug.log` |
| Debug Editor/placement/content/GUI/scene/plugin regressions | 12/12 passed | `out/AssetWorkspaceDebugRegression.log` |
| Release asset document/input, placement, docking and plugin regressions | 5/5 passed | `out/AssetWorkspaceFinalRelease.log` |
| Formatting, names and boundaries | 737 owned files; all 4 changed translation units; 701 C++ files / 35 modules passed | `tools/CheckStyle.py`, `out/AssetWorkspaceNaming.log`, `tools/CheckBoundaries.py` |
| Strict OpenSpec validation | 80/80 passed | `out/AssetWorkspaceOpenSpec.log` |

These follow-up runs include the newly added failing-before/passing-after scenarios. All changes remain uncommitted.

## Follow-up: existing Visual Studio warnings

The reported C4858 and C6031 share the intentionally ignored `try_acquire_for` result in the content-transition acceptance gate. `std::ignore` and its direct `<tuple>` include now express that intent, preserving the timeout fallback. C26817 came from copying a scene handle in the selection remapping loop; the loop now uses a const reference while retaining the existing build-then-swap behavior.

MSVC 19.50 `/analyze:only` with `EspXEngine.dll`, the C++ Core Check extension and explicitly enabled C6031/C26817 rules analyzed `EditorContentAcceptance.cpp` and `EditorStateTests.cpp` (which exercises the real selection header). Both completed without compiler warnings or analysis defects; logs and XML reports are under `out/WarningCleanupAnalysis/`.

Debug and Release Editor/state-test builds passed. Each configuration passed all four existing checks: `editor_content`, `editor_content_transition`, `editor_state` and `editor_multiselect`. Evidence: `out/WarningCleanup{Debug,Release}Build.log`, `out/WarningCleanup{Debug,Release}Tests.log` and the corresponding state-test build logs. Full formatting/path checks, semantic naming on the two affected translation units and `git diff --check` also passed. No warning suppression, runtime behavior change or commit was introduced.

## Follow-up: publication member-shadowing warnings

The two C4458 diagnostics in `FPublication::PreserveExternal` came from local `Bytes` and `Root` declarations hiding publication members. Renaming the locals to `AssetBytes` and `RootAsset` preserves the existing asset validation and reference publication behavior.

Visual Studio 2022 / v143 Debug and Release builds of `publication_tests` and `shared_asset_publication_tests` both recompiled `AssetPublicationGraph.cpp` without C4458. Each configuration passed `native_asset_publication`, `shared_asset_publication` and their `model_fixtures` prerequisite (3/3). Evidence: `out/PublicationWarning{Debug,Release}Build.log` and `out/PublicationWarning{Debug,Release}Tests.log`. Full formatting/path checks, semantic naming on the changed translation unit and diff whitespace checks passed.

Both builds reproduced the separate C4702 in TinyEXR's `LevelIndex`: every switch branch returns before the final `return 0`. Per the user's scope restriction, dependency code, the owned image adapter and warning settings remain unchanged; this third-party warning is intentionally retained. No Git commit was performed.

## Workspace notes

The first CPU run found stale generated fixtures containing pre-migration `.assets/<Id>-<Revision>.hasset` siblings with duplicate IDs. These generated files were preserved at `out/native-fixtures-before-asset-editors-20260922`, and `native_model_fixtures` regenerated current fixtures successfully. No content repository migration was performed; `F:/HyperionAssets` remains clean.

The OpenSpec change remains available for review. No Git commit or push was performed.

## Independent quality audit follow-up

An independent reviewer audited the complete uncommitted implementation against baseline `b6fffad7f230f7e46223ac054797f075c7857c37`, with an immutable 101-file hash inventory. The main agent independently confirmed all three findings: uncontained main-window save exceptions (QA-01/P1, tracked with the separate-window change), consumers added during asynchronous refresh retaining old model data (QA-02/P2), and pending encoding edits bypassing dirty/close protection (QA-03/P2). No third-party source was changed.

Refresh publication now detects uncaptured affected consumers and retains the changed identities for a follow-up refresh. A deterministic regression adds and duplicates models after the initial consumer capture, then verifies all instances receive the saved material while preserving names and transforms. Pending encoding is now part of workspace/tab dirty state; save-close actions and history wait for completion, while Discard remains an explicit cancellation. Acceptance checks this state after the actual GUI encoding choice and before the next Poll, including stale-save rejection and native-window/application exit protection.

Full Debug and Release builds passed. Six affected tests passed in each configuration: `editor_asset_documents`, `editor_asset_editors`, `editor_content`, `editor_content_transition`, `editor_content_startup`, and `scene_runtime_instance`. The first new save-shortcut test used an already-attached RenderSession; its test fixture was corrected to use an independent borrowed-resource session and rerun successfully. Final Debug evidence is split across `out/QualityAuditAssetEditors/FixesDebugTests.log`, `FixesDebugEditorTests.log` and `FixesDebugEditorDetailed.log`; Release evidence is in `FixesReleaseTests.log` and `FixesReleaseDetailed.log`. Editor GPU validation errors were zero in both configurations.

Style, module boundaries, semantic naming for 12 affected translation units, strict OpenSpec validation and diff whitespace checks passed. The original reviewer independently reran scene instance tests, asset document tests and full Editor asset acceptance on the 103-file repair snapshot; all three findings were closed with no new confirmed blocker. Reports, hash inventories and original logs are under `out/QualityAuditAssetEditors/`, including `IndependentReview.md`, `IndependentReReview.md` and `ReviewDisposition.md`.

Pending-encoding tab-save/root-switch branches were checked by code and state-transition analysis; the new automated pending-edit regression directly exercises workspace/native-window/application-close protection. Cross-monitor DPI and long-duration stress were not rerun. Changes remain uncommitted.

## Follow-up: stable preview during model Position dragging

Continuous model edits republish the transient preview scene. The conditional readiness text in `DrawPreview` appeared and disappeared above the image, changing the remaining content height, render target dimensions and camera aspect ratio. Readiness now appears in the native asset window's always-present, fixed-height status bar, reserved before dockspace layout. The async preview publication, live property editing and history behavior remain unchanged.

The new `EditorAssetPreviewAcceptance.cpp` scenario drives the actual Position X control through 48 pointer moves while holding the mouse button. It checks the preview rectangle on every drag/history frame, observes both preparing and ready states, verifies a single Undo restores the complete original model node data and clean state, verifies Redo restores the edited data, and restores the fixture before the existing material-slot acceptance continues. The test reproduced the original canvas change before the fix (`out/PreviewLayoutBeforeTests.log`). An initial post-fix coverage assertion sampled readiness only during pointer movement; it now includes settled frames before/after the drag so continuous rebuilding cannot hide the ready sample.

| Check | Result | Local evidence |
| --- | --- | --- |
| Debug / Release Editor builds | Passed | `out/PreviewLayout{Debug,Release}Build.log` |
| Debug affected acceptance | 3/3 passed: asset documents, asset editors and GUI docking | `out/PreviewLayoutDebugTests.log` |
| Release affected acceptance | Same 3/3 passed | `out/PreviewLayoutReleaseTests.log` |
| Position drag and Undo/Redo layout assertions | Passed in both configurations; GPU validation errors: 0 | `out/PreviewLayout{Debug,Release}Detailed.log` |
| Style / semantic naming / boundaries | 747 owned files; 5 affected translation units; 711 C++ files / 35 modules passed | `out/PreviewLayout{Style,Naming,Boundaries}.log` |
| Strict OpenSpec / diff whitespace | 81/81 passed; whitespace check passed | `out/PreviewLayoutOpenSpec.log`, `out/PreviewLayoutDiff.log` |

The captured Debug model preview (`out/build/debug/AssetEditor-Model.png`) was visually inspected: the readiness text is at the window bottom, outside the preview and properties dock panels. No third-party code was changed, and no Git commit or push was performed.
