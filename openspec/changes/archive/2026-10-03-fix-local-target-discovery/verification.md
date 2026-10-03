# Validation evidence

## Regression baseline

- Added a test with 160 valid local registrations and selected a target omitted from the 128-entry list. Before the fix, `automation_connection_tests` failed with `Target was not discovered; an explicit address can also be supplied`.
- The implementation separates `Find` from `List`, preserves connection admission/identity checking, and adds private process ownership metadata.

## Builds and regressions (2026-10-03)

- `tools/Build.ps1 -Preset debug` and `-Preset release`: full builds passed. Existing Debug RenderDoc-on and Release RenderDoc-off configurations were retained.
- Both configurations passed 17/17 tests selected with `^(transport_contracts|automation_.*|plugin_applications|offline_startup|process_output|editor_log)$`. GPU/desktop tests ran serially, with the Debug suite completed before the Release suite.
- Debug: `out/discovery-debug-regression.log`; Release: `out/discovery-release-regression.log`. Build logs: `out/discovery-debug-build.log`, `out/discovery-release-build.log`.
- The final review consolidated the private format version constant and expanded the unsupported-version fixture. Both configurations were rebuilt; Release's 17-test run used these final sources, and Debug's affected unit suite passed again (`out/discovery-debug-final-unit.log`).
- `automation_connections` covers an omitted target among 160 legacy records, exact probe/connect, 128-result and 1024-entry boundaries (including exact-limit completion), malformed/oversized/mismatched/unsupported records, legacy and unknown ownership, live process identity, PID reuse, read-only listing, publication cleanup and writer/changed-byte protection. Symlink rejection is exercised when the test user can create symlinks.
- `automation_discovery` exercises real Editor/JSONL/MCP and one-shot CLI attachment under both budgets, confirms the unchanged public target fields, retains a live owner's unreachable-endpoint record, distinguishes forced process termination from normal withdrawal, and verifies subsequent publication cleanup. Independent wrapper invocations create different roots and preserve exit code 77.
- Existing attachment tests cover explicit discovery-directory startup failure, disabled local automation, target isolation, shared scene history, save/reopen, admission and shutdown. No user discovery records were purged; fixtures live in per-run directories.

## Static and manual checks

- Full `CheckStyle.py` passed filenames/include casing and formatting; `CheckBoundaries.py` passed all source/module checks.
- Semantic clang-tidy/clang-query naming checks passed for all eight changed C++ translation units; the final two-file review change was checked again. Changed Python files parsed successfully.
- `git diff --check` and `openspec validate fix-local-target-discovery --strict` passed.
- Manual review: changed production C++ files stay below 500 lines (largest: ConnectionManager.cpp, 347); added/substantially changed functions stay below 100 lines. Native calls remain private adapters, process policy remains local discovery-owned, and connection resolution has no platform branch.
- Process identity, private format version and list budgets have one owning definition. Stop reasons have explicit stable enum values; wire reflection and derived truncation share the typed snapshot. No cached availability, polling cleanup, frame-path logs, operation-ID changes, target-schema changes or handshake changes were added.
- The private discovery test implementation is linked only into `automation_connection_tests`, not the runtime library. CTest IDs, working directories, fixtures and timeouts are preserved by the shared launch wrapper.

## Independent quality audit and repair (2026-10-03)

- A reviewer created without inherited conversation context audited the 28-file working-tree snapshot against baseline `37a5086216b01b54c5af52ed1c84b5e6e820c28a`. Source hashes stayed unchanged during the review. Reports and manifests are under `out/discovery-audit`.
- **DISC-01 / P2 / confirmed and fixed:** three Editor CTest registrations bypassed discovery isolation, and three mixed direct-launch/helper scripts initialized isolation only after their first GUI host. The main agent verified the CMake/default-plugin call chain and reproduced ambient discovery directory creation with `editor_content_startup` in a disposable parent environment (`IsolationBefore.log`). First-launch interception also confirmed all three standalone gaps (`StandaloneBefore.log`).
- Minimal repair: `editor_asset_editors`, `editor_content_transition` and `editor_content_startup` use the existing test wrapper; EditorLog, ModelPlacement and EditorRenderControls scripts call the existing isolation helper before their first subprocess. Original IDs, fixtures, arguments, directories, timeouts and labels remain intact. No production C++ or protocol changes were needed.
- Debug and Release Editor builds passed. Each configuration passed the six affected acceptance tests plus two fixtures, **8/8** (`DebugRegression.log`, `ReleaseRegression.log`). Both runs left their disposable parent discovery environments untouched (`ambientDiscoveryCreated=False`).
- Reviewer re-review verified the 32-file repaired snapshot with zero drift, independently checked all nine standalone environment branches (fresh / inherited wrapper / explicit failure override), and checked all 26 generated Editor commands per configuration had isolation. **DISC-01 closed; no new confirmed findings.** See `IndependentReview.md`, `ReReview.md` and `ReviewerIsolationAfter.log` in the audit directory.
- The reviewer independently ran the original core Debug discovery/lifecycle/attachment subset, 3/3 passed. Native cross-user access-denied and every possible filesystem race/reparse combination were not exhaustively exercised; these are documented validation limits, not additional confirmed defects.
- Repaired Python syntax, source/include paths, dependency boundaries, diff whitespace and OpenSpec strict validation passed. After re-review, only this evidence section was added; reviewed implementation files remain unchanged.

## Archive

All tasks are complete. Archived on 2026-10-03 under `openspec/changes/archive/2026-10-03-fix-local-target-discovery`. The `automation-transport` and `live-application-automation` delta specs were synchronized to the main specs. The validation and audit evidence above describes the reviewed implementation.
