# Quality audit

## Scope and versions

Independent review covered the 36 modified/new files for this capability and their direct call chains, based on `476e4e5759e60ac0c5b5b7616ebba537fa45cc7a`. The reviewer started without inherited conversation context and kept owned source read-only. Initial and repaired SHA256 manifests are in `out/SelectionShortcutsAudit/InitialSnapshot.json` and `RepairedSnapshot.json`.

The audit checked scene-wide Ctrl+A, Outliner range/anchor lifecycle, Viewport Shift equivalence, click/drag/input priority, shared SceneEditing validation and the typed automation/lifecycle contracts. No unrelated existing work was changed. The active OpenSpec change remains unarchived and uncommitted.

## Finding and disposition

| ID | Priority | Confirmed behavior | Repair and status |
| --- | --- | --- | --- |
| SS-01 | P2 | While navigating with RMB, ordered `[Ctrl+A down, RMB release]` events in one frame cleared the final navigation state before the shortcut guard, allowing unintended scene-wide selection. | Confirmed independently by the main agent through the actual FGui harness and source call chain, then reproduced in real Editor acceptance. The shortcut now preserves navigation ownership through any right-button transition in the current batch. Fixed and independently re-reviewed. |

Only two owned files changed during repair:

- `Source/Plugins/Editor/Private/EditorSelection.cpp`: reject the navigation transition batch before scanning for Ctrl+A. No persistent suppression state or domain/API change was added.
- `Source/Plugins/Editor/Private/EditorSelectionShortcutsAcceptance.cpp`: case 41 injects Ctrl+A before RMB release; case 42 verifies the original one-object selection, primary and authored-state invariants.

The new real-input regression first failed with unchanged product code: `selection size at step 42: expected 1, got 141`. GPU validation errors were zero, isolating the input-ownership defect. The identical regression passed after the repair.

## Post-repair validation

- Debug and Release Editor targets rebuilt successfully using `./tools/Build.ps1 -Preset <configuration> -Target hyperion_editor`.
- Both configurations passed 10/10 CTest cases (eight targeted tests plus two required model fixtures). Debug elapsed 70.72 seconds; Release elapsed 35.62 seconds. Their GPU groups ran sequentially.

```powershell
$TaskCTest = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
$TaskPattern = '^(editor_selection_shortcuts|editor_multiselect|editor_reparent|editor_clipboard|automation_selection_framing|scene_navigation|gui_input_and_data|editor_state)$'
& $TaskCTest --test-dir out/build/debug -R $TaskPattern --output-on-failure
& $TaskCTest --test-dir out/build/release -R $TaskPattern --output-on-failure
```

- The updated real Editor reports for Debug (`selection-shortcuts-s232sf3t`) and Release (`selection-shortcuts-rciu6fpb`) verify the exact failure sequence, with `selection_shortcuts_verified=true`, `document_dirty=false`, `validation_errors=0`, empty `scene_error` and 141 logical nodes.
- Source formatting, owned paths, module boundaries, `git diff --check` and strict OpenSpec validation passed after repair. No declaration names or public contracts changed in the repair.
- Original implementation validation is recorded in [validation.md](validation.md).

## Independent re-review and evidence

The original reviewer confirmed SS-01 resolved, checked the precise regression's failure/pass evidence and both configurations' raw reports, and found no new confirmed defect in the repair or its direct effects. All 36 files matched the repaired review manifest at that point. This audit summary and the validation follow-up were added afterward without further source changes.

Generated evidence under `out/SelectionShortcutsAudit` includes `InitialReview.md`, `ReReview.md`, the actual-FGui navigation-release harness and its output, `RegressionBeforeFix.log`, `DebugAfterFix.log` and `ReleaseAfterFix.log`. Full Editor reports remain under `out/editor-tests`.

This was an audit of the new capability and a targeted re-review of its repair. The reviewer independently ran the initial CPU tests, the original real Editor acceptance and the FGui harness, then inspected the main agent's repair validation evidence. Individual offscreen scrolling, Shift light hits and every native asset-window/modal transition were reviewed through source and existing coverage; they were not all manually exercised by the reviewer. No confirmed finding remains unresolved.
