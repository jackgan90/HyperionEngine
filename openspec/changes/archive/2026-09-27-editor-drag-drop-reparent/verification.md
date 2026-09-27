# Verification

Validated on Windows, Debug, on 2026-09-27. Acceptance feedback narrowed hierarchy drag sources to Outliner only. All eight implementation tasks are complete. This change remains active and unarchived; no Git commit was made.

## Build and automated checks

- Built `hyperion_editor`, `hyperion_automation_cli`, `automation_scene_tests`, `gui_tests`, `gui_docking_tests`, `scene_tests` and `scene_instance_tests` successfully.
- Initial implementation: all 11 targeted CTest cases passed: `automation_scene`, `scene_management`, `scene_runtime_instance`, `plugin_applications`, `gui_input_and_data`, `gui_docking`, `editor_acceptance`, `automation_attachment`, `editor_multiselect`, `editor_reparent` and `editor_placement` (230.67 seconds total).
- Outliner-only revision: rebuilt `hyperion_editor`; revised `editor_reparent` passed in 12.59 seconds, including viewport selection, absence of viewport hierarchy drags and subsequent Outliner reparenting for both a model and a light.
- Outliner-only regressions: `automation_scene`, `editor_acceptance` (including Gizmo, viewport picking and navigation), `editor_multiselect` and `editor_placement` all passed (159.82 seconds total).
- `tools/CheckStyle.py`, `tools/CheckBoundaries.py` and `git diff --check` passed. Semantic naming/local-declaration checks passed for 21 changed C++ translation units initially and the four translation units changed by the Outliner-only revision.
- `openspec validate --all --strict` passed all 96 items.

## Behavior exercised

- Shared batch operation: ordered selected-root normalization, preserved descendant hierarchy and world matrices, affine shear/negative/nonuniform scale, single undo/redo transaction, selection restoration, no-op behavior, detach, 256-node batch, and atomic rejection for invalid handles, duplicates, empty input, cycles, singular parents, busy state and stale revision/document.
- GUI adapter: external source delivery, tree hover expansion and edge scrolling.
- Actual Editor input: tree and filtered Outliner sources, viewport mesh/light selection synchronized to Outliner, rejection of viewport hierarchy initiation outside Gizmo handles and from the selected light marker, subsequent Outliner dragging of those selected objects, parent and scene-root targets, selected ancestor/descendant groups, no-op, self/descendant and singular-target rejection, Escape, focus loss, outside drop, right mouse, revision/document/selection invalidation, history and save/reopen.
- Actual CLI/MCP attachment: discovery and invocation of `scene.nodes.reparent`, preserved world transforms, no-op, rejection and one-step undo, followed by the existing persistence flow.
- Latest Editor report: `reparent_verified=true`, `document_dirty=false`, `validation_errors=0`, empty `scene_error`. The initial capture inspection confirmed Details has no Hierarchy section and Outliner displays the resulting parent/child structure.

## Local evidence

- Independent quality audit confirmed and resolved two P2 findings: ordinary affine-parent inverse rounding and filtered Outliner keyboard activation. Final re-review passed; see [audit.md](audit.md). Audit fixes passed Debug build, `automation_scene`, `automation_attachment`, `editor_multiselect` and the final `editor_reparent` run, plus style/naming/boundary and strict OpenSpec checks.

- Initial build/test logs: `out/ReparentBuild.log`, `out/ReparentTests.log`.
- Outliner-only build and acceptance logs: `out/ReparentOutlinerBuild.log`, `out/ReparentOutlinerAcceptance.log`.
- Outliner-only regression log: `out/ReparentOutlinerRegression.log`.
- Latest reparent acceptance report, screenshot and saved scene: `out/editor-tests/reparent-ahk5rytj/`.

The paths above are local validation artifacts, not required project configuration. Validation used the existing local Game content for the integration fixture. No Release build or interactive human acceptance is claimed.
