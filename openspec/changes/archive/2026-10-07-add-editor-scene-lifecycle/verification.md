# Verification

Date: 2026-10-07. Change remains active; no archive, spec sync, Git commit or push was performed.

## Builds and static checks

- Debug and Release all-target builds passed using `tools/Build.ps1` with the existing `out/deps` dependency tree. Final Editor builds also passed in both presets and in the existing Release `BUILD_TESTING=OFF` configuration.
- `python tools/CheckStyle.py` passed. Full naming/declaration checks passed for 796 translation units using merged configured Debug/testing and Release/production compile databases. The final menu adjustment also passed a focused clang-tidy and boolean-declaration check.
- `python tools/CheckBoundaries.py --build-dir out/build/debug` and the Release equivalent passed.
- Production `EditorAcceptanceBoundary.py` passed. The production executable rejects `--exercise-scene-lifecycle` with a controlled `Editor acceptance is unavailable: configure BUILD_TESTING=ON` diagnostic and exit code 1.
- Strict OpenSpec validation and `git diff --check` passed.

## Runtime and regression coverage

- Debug focused CTest selection: 12/12 passed, including required model fixtures, `automation_scene`, `editor_interaction_policies`, `editor_acceptance_boundary`, `editor_document_transitions`, `editor_content`, `editor_content_transition`, `editor_content_startup`, `scene_navigation`, `automation_scene_lifecycle` and `editor_scene_lifecycle`.
- Release focused selection: 14/14 passed, additionally covering `automation_capability_parity` and `editor_close_modal`.
- After the final menu compatibility adjustment, Debug `editor_acceptance`, `automation_scene_lifecycle` and `editor_scene_lifecycle` passed (3/3). Full Editor acceptance took 154.14 seconds and covers existing open, document persistence, views, gizmos, picking, framing and error recovery.
- After the final menu adjustment, Release `automation_scene_lifecycle` and `editor_scene_lifecycle` passed (2/2), recorded in `out/SceneLifecycleFinalReleaseTests.log`.
- GUI acceptance clicks the real menus and Save/Discard/Cancel controls for both New and Close, including title dismissal, Save As cancellation and named/untitled persistence. It also verifies Ctrl+S on a zero-object scene, auxiliary-window blocking/restoration, retained asset identity, and real admitted-save cancellation without a revived close.
- Attached MCP and JSONL acceptance verifies discovery/schema/provider availability, empty native save/reopen, dirty/default and stale request rejection, invalid decisions, asynchronous IO failure, explicit save/discard continuation, old document invalidation, retained dirty asset documents and asset saves/screenshots after scene close.
- Lifecycle logs and final GUI reports show zero GPU validation errors. The closed viewport was visually inspected; it presents New/Open, while Outliner and Details show no open scene.

## Resolved validation issues

- Closed-scene runtime testing found an unconditional Details revision query; the panel now handles the closed state before accessing the instance.
- New GUI tests use their own readiness settle budget because the existing ReadyFrames counter only advances for named scenes. Asset-window restoration is observed after window-group synchronization.
- Older generated native fixtures contained duplicate asset IDs. They were preserved at `out/tests/fixtures/native-scene-lifecycle-backup-20261007`; clean fixtures were regenerated and navigation passed.
- Full Editor acceptance initially exceeded its aggregate timeout while builds and checks ran concurrently. Both subsequent isolated runs passed without increasing the timeout.

## Ownership and compatibility review

Lifecycle semantic actions and dirty decisions belong to SceneEditing; validation, save continuation and resource retirement stay in the Editor host. GUI and catalog bindings share that host, and session polling can advance admitted work during shutdown draining. Scene close preserves content roots and independent asset documents, and guards render publication, asset refresh, Details and lighting access. Existing operation IDs/schemas, empty-path `scene.open` behavior, persistence formats and application/plugin lifecycle contracts are retained. Save-in-progress opening behavior is also retained; New/Close remain blocked during a scene save.

## Independent quality audit

The 2026-10-07 independent review and primary-agent verification found no confirmed defect; no source repair was needed. A suspected persistent modal after synchronous save failure was rejected using a multi-frame ImGui probe and direct cleanup-call-chain verification. Independent CPU regressions passed 3/3, and both agents checked the reviewed file hashes with zero mismatches. Existing runtime evidence was inspected without repeating the full builds or GPU suites. Scope, disposition, raw evidence and verification limits are recorded in [audit.md](audit.md).

## User acceptance follow-up: closed Place Object panel

User acceptance identified repeated `Wait for a valid scene document` messages in Place Object after closing an edited scene. The shared preparation result correctly prevented placement, but the GUI rendered its scene-unavailable reason as a per-object preparation prompt even in the normal closed state. The panel now uses the instance's typed closed status to hide preparation tooltips, inline prompts and footer feedback while retaining disabled entries. Scene retirement also clears the old placement status; open-scene diagnostics and shared GUI/automation placement admission retain their existing behavior.

- Debug and Release Editor builds passed: `out/SceneLifecyclePlacementDebugBuild.log` and `out/SceneLifecyclePlacementReleaseBuild.log`.
- Full `CheckStyle.py` passed, with focused semantic naming/declaration checks for both changed translation units: `out/SceneLifecyclePlacementStyle.log` and `out/SceneLifecyclePlacementNaming.log`.
- Debug `automation_scene_lifecycle`, `editor_scene_lifecycle` and `editor_placement` passed **3/3**: `out/SceneLifecyclePlacementDebugTests.log`. Release `editor_scene_lifecycle` passed **1/1**: `out/SceneLifecyclePlacementReleaseTests.log`.
- The new Debug closed-scene capture at `out/build/debug/SceneLifecycleGui/Closed.png` was visually checked: the object entries remain disabled, with no repeated waiting message or placement footer. The lifecycle report retains `placement_unavailable=9`, confirming that suppressing presentation does not enable placement. GUI runs record zero GPU validation errors.
- Strict OpenSpec validation and `git diff --check` passed. The change remains unarchived and uncommitted.
