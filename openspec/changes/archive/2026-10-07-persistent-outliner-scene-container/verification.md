## Scope and version

Baseline: `2b51b4ae4f2041ebca3ec9c438ecea7816e87f53`. Validation performed on 2026-10-07 against the uncommitted worktree. The change implements an Editor-only Scene container; scene records, object handles and `scene.nodes.reparent` remain unchanged. Implementation and review initially completed without archival or commit; the user subsequently authorized OpenSpec archival followed by a local Git commit on 2026-10-07. Push remains outside the requested scope.

## Build and checks

- Debug `hyperion_editor`, `editor_reparent_controller_tests`, `gui_tests`, `hyperion_automation_cli`, `automation_scene_tests` and `scene_tests` built successfully. Logs: `out/SceneContainerValidation/RepairBuild.log` and `ScrollBuild.log`.
- The selected 12 regressions passed across the final applicable runs: `automation_scene`, `editor_reparent_controller`, `editor_acceptance_boundary`, `scene_management`, `plugin_applications`, `gui_input_and_data`, `automation_attachment`, `editor_multiselect`, `editor_selection_shortcuts`, `editor_reparent`, `code_style_paths`, and `code_style_sizes`.
- `RepairTests.log` records 11 passes and the intermediate selection-shortcuts failure. After the test-only repair, `ScrollTests.log` records the successful complete selection-shortcuts rerun. Other production sources were unchanged between these runs.
- Scoped source formatting and semantic naming passed for all changed C++ files / 9 translation units. Naming and formatting were rerun for the final acceptance repair. Module boundaries, `git diff --check` and strict OpenSpec validation passed.
- Controller tests compare permanent Scene/object bounds before, during and after root no-op/cancellation; verify scene-row exclusion, empty scene, collapse/explicit expansion, document identity, node/root delivery and history behavior. Actual Editor reparent acceptance additionally compares drag-start layout and retains KeepWorld, selection, cancellation and persistence checks.
- GUI tests exercise actual tree-arrow mouse clicks with 0, 1 and 3 ancestor levels at UI scale 1 and 2, including collapse/reopen and empty leaf toggle bounds. The actual Editor capture was inspected: Scene is the permanent first table row with Type `Scene`, and objects are indented beneath it.

## Quality audit and repairs

The `quality-audit` skill was used with a reviewer created without conversation inheritance. The reviewer checked both initial and repair file hashes before and after read-only review. Snapshots and raw logs are under `out/SceneContainerValidation/`.

- **SC-01, P2, confirmed:** the existing selection-shortcuts acceptance inferred the tree-arrow position from the entire row's left edge. The Scene scope adds indentation, while `SpanAllColumns` keeps that row edge at the table boundary. The main agent independently checked the GUI/native arrow geometry and reproduced `tree child did not fold`.
  **Repair:** GUI now optionally reports the actual tree toggle region, the Editor publishes that observation, and acceptance clicks its center. Leaf regions are empty. Original flags, click handling and default callers retain their behavior. Reviewer reinspection confirmed the region calculation matches the current GUI adapter. The original fold assertion subsequently passed.
- **Follow-on endpoint visibility, confirmed by the main agent:** the same acceptance then tried to Shift-click a lower row outside the table viewport after clearing search. Target center was `(1433, 441.5)` and no gesture for that object began (`HitTests.log`). The new Scene row exposes the old fixture's assumption that all target rows fit without scrolling.
  **Repair:** after folding, acceptance supplies a real mouse-wheel event to bring the lower rows into view. It also checks endpoint gesture admission before release. All existing folded-range, primary-selection, drag arbitration, cancellation, deletion and persistence assertions remain. The complete acceptance passed (`ScrollTests.log`). This changes only the test-enabled input fixture.

Final targeted review completed against `FinalReviewSnapshot.json` (25 files, matching hashes before and after review): SC-01 is closed, the follow-on endpoint repair preserves all original assertions, and no new confirmed issue was found. After that review, only task completion, delivery metadata and this conclusion were updated; reviewed source files are unchanged. Reviewer validation is source/log inspection; the main agent ran the GPU acceptance tests. Search-triggered expansion and collapsed-row hover expansion are supported by the checked call chain, but were not independently reproduced end to end in this change.

## Archive and local delivery

On 2026-10-07, the completed change was archived to `openspec/changes/archive/2026-10-07-persistent-outliner-scene-container/`. The `editor-hierarchy-drag-drop` main specification was synchronized with one added requirement and one modified requirement. `openspec validate --all --strict` passed all 136 items after synchronization, and no active changes remained. The reviewed source hashes still match the final audit snapshot. Local commit delivery uses the exact archived change, synchronized specification, implementation, tests and documentation; generated `out/` evidence is excluded from Git.
