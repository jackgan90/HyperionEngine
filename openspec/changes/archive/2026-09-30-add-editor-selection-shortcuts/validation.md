# Validation

Validated on 2026-09-30 in the uncommitted working tree based on `476e4e5759e60ac0c5b5b7616ebba537fa45cc7a`. This change remains active; no archive, staging or Git commit was performed.

## Builds and configurations

- `./tools/Build.ps1 -Preset debug`: complete build passed. The final acceptance-input adjustment was rebuilt with `./tools/Build.ps1 -Preset debug -Target hyperion_editor`.
- `./tools/Build.ps1 -Preset release`: complete build passed.
- Both configurations use Ninja and Visual Studio 18 BuildTools MSVC x64. Debug has Debug UI and RenderDoc enabled; Release has both disabled. Tracy is disabled in both configurations. No build-selection or plugin-dependency contract changed.

## Regression commands and results

CTest executable used: `C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe`.

```powershell
$TaskCTest = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
$TaskPattern = '^(editor_state|editor_acceptance|editor_multiselect|editor_placement|editor_content_transition|scene_navigation|plugin_applications|editor_reparent|editor_clipboard|editor_outlines|automation_scene|automation_selection_framing|automation_attachment|automation_contracts|automation_connections|transport_contracts|gui_input_and_data|plugin_runtime|editor_selection_shortcuts)$'
& $TaskCTest --test-dir out/build/debug -R $TaskPattern --output-on-failure
& $TaskCTest --test-dir out/build/debug -R '^(scene_management|transform_gizmo)$' --output-on-failure
& $TaskCTest --test-dir out/build/release -R '^(editor_state|editor_acceptance|editor_multiselect|editor_placement|editor_content_transition|scene_navigation|plugin_applications|editor_reparent|editor_clipboard|editor_outlines|automation_scene|automation_selection_framing|automation_attachment|automation_contracts|automation_connections|transport_contracts|gui_input_and_data|plugin_runtime|editor_selection_shortcuts|scene_management|transform_gizmo)$' --output-on-failure
```

- Debug: 22/22 passed in the main group (including required content/model fixtures), plus 2/2 pure SceneEditing/transform regressions. Main group elapsed 283.31 seconds.
- Release: 24/24 passed, including the same fixtures and regressions, in 70.63 seconds.
- GPU/desktop tests ran serially, with the two configurations' GPU groups also separated. The new real Editor input test checked 141 logical nodes, both focused panels, retained primary, filtered/folded inclusive ranges, consecutive/reverse ranges, additive ranges, Shift toggle/miss, text/foreign-window/modal/navigation/repeat guards, press-time modifiers and cancelled hierarchy drag. Its report asserts unchanged authored state and zero GPU validation errors.
- Debug new-test report: `out/editor-tests/selection-shortcuts-g74k3vgk/selection-shortcuts.json` (`selection_shortcuts_verified=true`, `document_dirty=false`, `validation_errors=0`, empty `scene_error`, `nodes=141`). Reports, logs and captures remain generated output under `out`.
- Automation tests verified `api.search`, `api.describe`, typed invocation, the bounded reflected summary, more than 128 explicit handles, atomic invalid/stale/busy rejection and unavailable-provider behavior. `automation_selection_framing` additionally exercised real CLI/MCP clients sharing the Editor selection and authored-state invariants.
- Existing regressions covered picking/gizmo priority, multi-selection edits/history, document transitions, outlines, keyboard Outliner activation, hierarchy drag/reparent, clipboard, placement, provider absence and plugin cleanup.

## Static and specification checks

- `python tools/CheckStyle.py --paths-only`: passed.
- `python tools/CheckStyle.py`: passed.
- `python tools/CheckStyle.py --naming --build-dir out/build/debug`: passed for 632 translation units.
- `python tools/CheckBoundaries.py`: passed for 908 files across 38 modules.
- `git diff --check`: passed.
- `openspec validate add-editor-selection-shortcuts --strict`: passed.

Only affected owned files were formatted. Newly added or substantially changed functions were reviewed for the repository's function-length, naming, module-boundary and UI-independent domain requirements. Selection-specific request capacity was expanded; unrelated edit-batch limits and existing transport budgets remain in place.

## Quality audit follow-up

An independent quality audit subsequently confirmed and repaired SS-01: Ctrl+A during navigation followed by RMB release in the same input batch could incorrectly select all objects. The new real Editor regression failed before repair (`expected 1, got 141`) and passed afterward. The two-file repair rebuilt Debug/Release Editor targets and passed 10/10 targeted cases in each configuration, with zero GPU validation errors in the new acceptance reports. Formatting, boundaries, diff and strict OpenSpec checks passed; independent targeted re-review confirmed the finding resolved with no new confirmed defect. The full groups above were run before this audit repair; post-repair scope, commands and evidence are in [audit.md](audit.md). The change remains active with no archive or Git commit.
