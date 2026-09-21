# Verification

Validated on Windows / D3D12 with the debug layer enabled, 2026-09-21. All changes remain uncommitted; the OpenSpec change is not archived.

## Delivered behavior

- File starts the menu bar. Open uses the native Windows directory selector; Recent retains five unique successful roots and startup restores the last root unless explicit mounts override it.
- The selected directory itself maps to `/Game`; `/Engine` remains unchanged. Invalid restoration leaves Game unmounted with an error.
- Content Browser provides a resizable directory tree and immediate-child folder/file grid, hasset-only filtering, navigation refresh and explicit Refresh. Internal asset visibility is separate from loading permissions.
- Open Scene discovers native scene types recursively from the current root, including uncataloged scenes in arbitrary subdirectories. Typed double-click uses the same scene-opening path or an unsupported/read-error modal.
- Root changes preflight twice around user decisions. Save targets the old root; cancellation, invalid candidates and save failure preserve the document. Accepted switches join old work, retire GPU resources, clear the document/selection/history/previews and replace mounts and catalogs at a quiescent boundary.
- Candidate replacement preserves IO/provider destruction order. Failures after teardown begins use the controlled application failure path.
- The default browser dock is below Viewport, preserving full-height Outliner/Details. Existing saved layouts are retained; Reset Layout applies the new arrangement.

## Builds and checks

| Check | Result | Evidence |
|---|---|---|
| Full Debug build | Passed | `out/ContentBrowserBuild.log` |
| Full Release build | Passed | `out/ContentBrowserReleaseBuild.log` |
| Broad affected Debug regression | 43 of 44 passed initially; the remaining editor interaction test passed after the layout/async UI fixes below | `out/ContentBrowserRegression.log` |
| Final affected Editor/GUI Debug regression | 13/13 passed, including the full editor interaction suite | `out/ContentBrowserFinalTests.log` |
| Release feature/IO/preferences/plugin tests | 7/7 passed | `out/ContentBrowserReleaseTests.log` |
| Native folder selector | Cancel and selection of a Unicode directory passed | `folder_dialog` in Debug and Release |
| Assets disabled | Idle profile exits normally; explicit exercise request fails with an unavailable-output diagnostic | `out/build/debug/editor-content-tests/assets-disabled*.log` |
| Source formatting, paths, semantic C++ naming | Passed; all changed translation units checked, with final edits rechecked | `out/ContentBrowserStyle.log`, final CheckStyle invocation |
| Module boundaries | Passed | `python tools/CheckBoundaries.py` |
| OpenSpec strict validation and diff whitespace | Passed | `openspec validate add-editor-content-root-browser --strict`, `git diff --check` |

Broad regression coverage includes IO, native assets, scene ownership, asynchronous failure paths, materials, resource retirement, GUI input/docking/rendering, plugin selection/absence, capture, editor selection/placement and shutdown validation. Only affected tests were rerun after each correction.

## Feature acceptance

`editor_content` creates isolated A/B asset roots beneath each build directory. It verifies directory filtering, empty folders, recursive uncataloged scene discovery, unsupported type classification, cancellation, A/B/A asset-cache isolation, catalog replacement, old-root writes, invalid candidates and recent-root persistence.

`editor_content_transition` exercises actual GUI mouse input for save/cancel/discard, folder and scene double-click, and internal-asset toggling. It additionally verifies a read-only scene save failure preserves the dirty document, an invalid root preserves the document, and switching immediately after starting a scene load leaves a clean empty workspace. Both Debug and Release finish without GPU validation errors.

`editor_content_startup` verifies restored roots, explicit mount precedence and invalid saved roots using isolated preferences and reports. Existing IO coverage also verifies that directory listing reports excluded links without following them when Windows permits symlink creation.

The initial broad run exposed asynchronous Open Scene status text moving click targets; the status row now has stable height, and the exercise waits for scene discovery. The subsequent document exercise identified a clipped Details checkbox after enlarging the browser dock. The default layout now preserves the right column's height. The original assertions remain in place, and the complete editor suite subsequently passed.

Screenshots inspected:

- `out/build/debug/editor-content-tests/Browser.png`: empty document after root replacement, root tree, folders and native-file tiles.
- `out/build/debug/editor-content-tests/Editor.png`: actual Sponza editor with the new menu/browser and usable Details panel.

## Display path follow-up

Open Scene now displays root-relative labels and editable paths (`Scenes/Sponza.hasset`); relative input resolves under `/Game`. Content Browser displays `All` and `All/Scenes` while retaining internal package identities. Debug and Release editor builds passed. Existing `editor_content` and `editor_content_transition` passed, and the menu/double-click/reopen exercise confirmed `/Game/Scenes/Sponza.hasset`, three open attempts including the initial missing scene, and zero GPU validation errors. Formatting and semantic naming checks passed. Evidence: `out/ContentPathLabels*.log` and `out/build/debug/editor-content-tests/LabelsEditor.png`.

## Independent audit follow-up

The independent review covered the 64-file uncommitted snapshot against `41ca002913449a52b29fc4371b1aa88d8fb3c41d`. Its report is `out/ContentBrowserAudit/IndependentReview.md`; the main-agent disposition and the subsequent targeted review are recorded alongside it.

Two related dialog-state defects were confirmed and minimally repaired: dismissing the unsaved-changes title bar during save-and-switch now cancels the transition while allowing the old-root save to finish, and Discard honors application close even when a root candidate is pending. The first was a formal independent finding; the second was independently confirmed from a reviewer follow-up observation.

`editor_content_transition` now holds real IO work pending, clicks the title-bar X through normalized mouse events, releases the save and verifies both the mounted root/document and saved bytes remain in A. It then overlaps a dirty root prompt with application close, clicks Discard, and requires a clean close. Debug and Release each passed `editor_content` plus `editor_content_transition`, with zero final GPU validation errors. Evidence: `out/ContentBrowserAudit/{Debug,Release}Tests.log` and `{Debug,Release}TestDetail.log`. Both editor builds, formatting, and semantic naming for all three changed translation units passed. No commit or archive was made.

The existing Debug document save/reopen acceptance also passed (`out/ContentBrowserAudit/Document.json`: document verified, clean, zero validation errors; final graphics validation also zero).

## Deliberate limits

No filesystem watcher, asset thumbnails, import UI, file mutation tools or non-scene asset editors are added. Metadata discovery validates bounded native files asynchronously without loading dependency graphs; large roots can take time to scan. Root switching uses a synchronous retirement barrier and may briefly pause while old work/GPU resources finish. Visibility does not make `.assets` dependencies disposable.
