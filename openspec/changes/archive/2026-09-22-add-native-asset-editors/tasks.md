## 1. Retire persistent Catalog assets

- [x] 1.1 Replace Catalog transport and persistence with discovered in-memory index APIs; migrate all owned callers and remove the CLI writer/type.
- [x] 1.2 Update index tests and current documentation; verify discovery, duplicate IDs, moved references and obsolete-type rejection.

## 2. Document and save foundations

- [x] 2.1 Add native save preconditions and saved-asset publication metadata with regression coverage.
- [x] 2.2 Implement independent asset drafts, transactional history, baseline/save state, typed opening and reference selection.
- [x] 2.3 Add Gui tab/focus support and integrate asset workspace commands, protected close/exit/root transitions and lifecycle cleanup.

## 3. Texture editor

- [x] 3.1 Implement texture inspection, channel/mip/cube/HDR display, pan/zoom and pixel values.
- [x] 3.2 Implement name and RGBA8 2D encoding edits with exact mip undo/redo and save/reload tests.

## 4. Model and sky previews

- [x] 4.1 Implement shared independent 3D preview scenes, camera navigation, framing and render-target lifecycle.
- [x] 4.2 Implement model hierarchy/statistics, name/local-transform/slot editing and native material replacement.
- [x] 4.3 Implement sky and lighting previews, product/SH inspection and name editing.

## 5. Material editor

- [x] 5.1 Implement typed parameter/value/default projection, numeric/color/simple-leaf and sampler editing, reset and scope restrictions.
- [x] 5.2 Implement native texture selection, UV controls and actual shader preview on selectable geometry, including custom shaders.

## 6. Saved dependency refresh

- [x] 6.1 Add reusable asynchronous scene dependency refresh without scene reload, retaining authored overrides and stable handles.
- [x] 6.2 Refresh open previews after saves and rebind scene history against current published dependencies; isolate unsaved drafts and reject stale completions.

## 7. Validation and delivery

- [x] 7.1 Add real Editor input acceptance for all current asset types, history/save/reference flows, dirty workspace close and refresh with existing scene edits.
- [x] 7.2 Run affected Debug/Release builds, CPU/GPU/desktop regressions, style/naming/boundary checks and strict OpenSpec validation; fix failures.
- [x] 7.3 Update Editor/native asset documentation and record verification evidence; leave all changes uncommitted.

## 8. Asset workspace interaction regressions

- [x] 8.1 Reproduce and repair immediate clean/discard tab close without invalidating the current GUI frame or exiting the Editor; cover all preview types.
- [x] 8.2 Reproduce and repair Place Object window dragging with asset tabs active, preserving placement cancellation and unrelated GUI gestures.
- [x] 8.3 Run Debug/Release interaction and affected lifecycle regressions, style/naming/boundary checks and strict OpenSpec validation; record evidence without committing.

## 9. Existing Visual Studio warnings

- [x] 9.1 Explicitly discard the acceptance semaphore result and avoid selection-loop handle copies; verify MSVC diagnostics, Debug/Release builds and affected existing tests without changing behavior or committing.
- [x] 9.2 Remove publication-local member shadowing and verify Visual Studio Debug/Release builds and existing publication regressions. Leave TinyEXR's C4702 unchanged, including adapter/build warning settings, as requested; do not commit.

## 10. Independent quality audit

- [x] 10.1 Include scene consumers added during asynchronous asset refresh, preserving authored edits and adding a deterministic regression.
- [x] 10.2 Protect pending texture encoding edits across tab/window/exit/root close and save actions; verify deferred completion and explicit discard.
- [x] 10.3 Validate the bounded fixes and obtain targeted independent re-review; record findings and evidence without committing.

## 11. Stable asset preview layout

- [x] 11.1 Reproduce continuous model Position dragging and keep preview readiness text outside the preview's content layout.
- [x] 11.2 Verify stable canvas bounds through ready/preparing transitions and atomic Undo/Redo; run affected Debug/Release checks and retain uncommitted changes.
