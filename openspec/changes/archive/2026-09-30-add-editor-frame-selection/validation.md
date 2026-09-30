# Validation

Validated on 2026-09-30 on Windows / D3D12. At completion of implementation and independent audit, this change remained active without archive, main-spec synchronization, staging, commit or push. The user subsequently authorized archive followed by a local Git commit.

## Implementation

- F frames the shared selection from Viewport, Outliner or Details. It preserves orientation/FOV, fits temporary clipping planes, and uses the center of the union of selected subtree bounds.
- Renderer owns bounds fitting and subtree traversal, including affine transforms, ancestor/descendant de-duplication, SourceNode/SourcePrimitive boundaries, hidden/disabled models and stable position-based non-geometric bounds.
- GUI and reflected `view.frame_selection` call the same `ISceneViewport` provider. Document/revision, readiness, busy interaction and authored-camera preview are validated before atomic camera publication.
- Main-toolbar Frame Scene was removed. Home and `view.frame_scene` remain supported. Empty selection does not move the camera or change the document.

## Builds

Both full-target builds passed, followed by incremental builds for the final navigation regression additions:

```powershell
.\tools\Build.ps1 -Preset debug
.\tools\Build.ps1 -Preset release
.\tools\Build.ps1 -Preset debug -Target scene_navigation_tests
.\tools\Build.ps1 -Preset release -Target scene_navigation_tests
```

Build logs: `out/FrameSelectionDebugBuild.log`, `out/FrameSelectionReleaseBuild.log`, `out/FrameSelectionDebugNavigationBuild.log`, and `out/FrameSelectionReleaseNavigationBuild.log`. Existing build directory permissions required running the build/test commands outside the sandbox; approval review accepted those commands.

## Automated acceptance and regressions

Debug and Release each passed the same 17 distinct CTest entries, including the two required fixture setup tests. This is targeted validation, not a claim that the entire CTest inventory ran.

| Coverage | Passing tests |
| --- | --- |
| Framing, shared service, real GUI and CLI/MCP | `scene_navigation`, `automation_scene`, `editor_acceptance`, `automation_selection_framing` |
| Engine/window input and GUI | `window_event_routing`, `window_activation_input`, `gui_input_and_data`, `gui_docking` |
| Existing Editor behavior | `editor_multiselect`, `editor_outlines`, `editor_render_controls` |
| Provider absence and lifecycle | `plugin_runtime`, `plugin_applications` |
| Repository checks and fixtures | `dependency_boundaries`, `code_style_paths`, `model_fixtures`, `native_model_fixtures` |

- Renderer regression covers ordered/reversed multi-selection, overlapping subtrees, transformed bounds, hidden/disabled primitive selection, non-geometric fallbacks, loading rejection, terminal model-load failure fallback, invalid-input atomicity, and frustum containment for narrow/wide aspects and both depth conventions.
- The real Editor exercise injects mouse/key/focus events through the normal input path. It covers Outliner selection/multi-selection, Viewport picking, Details focus, F repeat/modifier/text/popup/focus-loss/RMB guards, authored-camera preview, empty selection and retained Home behavior. Authored snapshot, revision, dirty state and history remain unchanged by framing. GUI captures and reports require zero graphics validation errors.
- Real attached CLI/MCP connections verify operation search/description schemas, shared camera visibility across connections, selection-order independence, single/multi/empty behavior, stale document/revision and malformed input, authored-camera preview isolation, camera/empty-group fallbacks and normal shutdown. The no-provider catalog test verifies controlled `unavailable` failure.
- The first GUI framing run exposed a source-less Cube test fixture when serializing its authored snapshot. The fixture now uses the registered primitive asset through a scene node; the isolated exercise and full Debug/Release acceptance passed afterward.

Implementation validation logs: `out/FrameSelectionDebugFeatureTests.log` (6/6), `out/FrameSelectionDebugNavigationTests.log` (3/3 after final test additions), `out/FrameSelectionReleaseContracts.log` (4/4), and `out/FrameSelectionReleaseFeatureTests.log` (13/13). The separate Debug input/Editor/plugin regression group passed 11/11. An isolated Debug GUI run passed at `out/editor-tests/framing-0lfv8zde`; its capture was visually checked for removed toolbar button, viewport composition and navigation guidance.

## Style, boundaries and specification

- `python tools/CheckStyle.py`: owned filename/include casing and formatting passed for all 943 source files.
- `CheckStyle.check_naming` with the Debug compile database passed for all 17 changed/new C++ translation units, including semantic boolean-prefix checks. Headers are covered through their translation units.
- `python tools/CheckBoundaries.py`: all 906 checked files across 38 modules passed.
- `git diff --check`: passed. No files were staged.
- `openspec validate add-editor-frame-selection --strict`: passed.

No asset, persistence-schema or transport migration is required. Existing conservative bounding-sphere padding and a 0.5-unit fallback half extent are intentional framing policies documented in the design.

## Independent quality audit and corrections

An independent reviewer used the user-requested `gpt-6-astra` model with `high` reasoning, without inherited conversation context. Initial scope was baseline `de1e3455a434c9f19b04f8e61122461e9bb9686f` plus the 36 unstaged/new files frozen in `out/FrameSelectionAudit/InitialSnapshot.json`. The main agent kept those files unchanged during initial review and independently verified both P2 findings in `out/FrameSelectionAudit/IndependentReview.md`.

| Finding | Confirmation and repair |
| --- | --- |
| FS-01: full-scene framing retained selection-sized clipping | A real Editor/CLI sequence with cubes at x=-300/+300 produced object depths 943.84 beyond Far 11.38 after selection framing followed by full-scene framing. Editor `FrameScene` now explicitly fits temporary clip planes. The reusable Renderer default remains unchanged. Repeating the sequence produced Far 3948.85 with both centers inside the clipping range. Evidence: `FramingSequenceBefore.json` and `FramingSequenceAfter.json` under the audit directory. |
| FS-02: shared provider admitted RMB navigation / ordinary popups | The main agent verified the shortcut/provider admission mismatch and added a real GUI regression that invokes the shared provider while RMB is held. It failed before the fix with `shared service admitted framing during active interaction` (`out/FrameSelectionAudit/RedGui/framing.log`). `FrameSelection` now rejects active camera navigation and general popup ownership before camera publication, without changing the global document busy policy. The regression checks busy, unchanged camera and uninterrupted RMB state; the ordinary-popup case also asserts that the document itself was not already busy. |

Both full-target Debug and Release builds passed after the corrections (`out/FrameSelectionAudit/DebugBuild.log`, `ReleaseBuild.log`). Each configuration passed the same 8 CTest entries: `automation_scene`, `scene_navigation`, `gui_input_and_data`, `editor_acceptance`, `automation_selection_framing`, `editor_render_controls` and their 2 model fixture setup tests. Logs: `out/FrameSelectionAudit/DebugTests.log` (8/8), `ReleaseTests.log` (8/8). GUI Home regression now checks all full-scene bounds corners in the frustum for both depth conventions; CLI acceptance uses widely separated cubes and checks all cube corners against Near/Far. Existing document/preview/isolation and graphics-validation assertions also passed.

Full formatting/path and dependency-boundary checks passed again, along with semantic naming for the two C++ translation units changed by these repairs and strict validation of the updated design/spec. The original independent reviewer completed targeted re-review against `out/FrameSelectionAudit/FixedSnapshot.json`: all 36 hashes matched, both findings were resolved, and no new confirmed defect was found. The reviewer independently recomputed the real probe's 16 cube corners: 0/16 were inside the clipping range before repair and 16/16 afterward. The reviewer inspected the red/green GUI and Debug/Release logs without rerunning GPU tests. See `out/FrameSelectionAudit/TargetedReReview.md`; task 5.3 is complete.

The final audit administrative update only checked off task 5.3 and recorded that review result. `out/FrameSelectionAudit/FinalSnapshot.json` verifies that all source, test, design/spec and feature-document hashes still match the reviewed fixed snapshot. No archive, staging, commit or push was performed during that audit phase.

## Archive and delivery

On 2026-09-30, the user authorized OpenSpec archive followed by a local Git commit. All 16 tasks and all four artifacts were complete. The 36-file final audit snapshot matched the working tree before archive. `openspec archive add-editor-frame-selection --yes` synchronized all five added requirements to `openspec/specs/editor-selection-framing/spec.md` and archived the change at `openspec/changes/archive/2026-09-30-add-editor-frame-selection/`.

Post-archive `openspec validate --all --strict` passed all 102 specifications, and `openspec list --json` reported no active changes. Archive preparation only supplies the main specification purpose and updates this historical delivery record; reviewed source, tests, design and requirements remain unchanged. Local build, test, audit and commit-manifest evidence remains under ignored `out/`. Push is outside the authorized delivery scope.
