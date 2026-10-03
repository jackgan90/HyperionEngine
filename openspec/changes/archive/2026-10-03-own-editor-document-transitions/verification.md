# Verification: owned Editor document transitions

## Source and scope

- Baseline: `684b54fe26b73dd22b3af7fcc31b10c9b8f78b5a`; validation date: 2026-10-03.
- This change owns Editor document transition decisions and migrates their callers. It does not change Runtime/Application, ContentRootService, plugin selection, transports, reflection schemas or persisted keys.
- Implementation and validation were initially delivered as an active, uncommitted change. The user subsequently authorized OpenSpec synchronization/archive followed by a local Git commit. No push was requested.

## Behavior and manual review

- Root, close and open requests are private. Targets and phases are typed; the close protocol's idle/saving/failed/closing strings are read-only projections. GUI modal presentation is a separate private typed state.
- Hosts supply `FEditorSaveProgress` facts and execute IO/window/content effects. The owner decides admission, save continuation, failure, discard priority, cancellation and one-shot consumption. No mutable transition fields or the old discard enum remain in consumers.
- Close-over-root-over-open discard priority is retained. Accepting save-and-close retires obsolete replacement intents, including before a later asynchronous close-save failure. New dirty work between close acceptance and native exit re-enters the close decision while preserving the existing closing status.
- Cancelling a button, title-bar decision or Save As clears continuation authority. Admitted saves continue against their original documents; late completion/failure cannot restore a cancelled switch or exit. Save As distinguishes submission from dismissal.
- Root commit waits for active saves and asset edits. The host still calls ContentRootService Prepare and Commit, preserving final participant dirty/busy/generation checks and retirement. The pending intent is consumed before final service preflight.
- Existing shared scene/close host services remain the GUI and automation boundary. No operation IDs, schemas, revisions or close reply/lifetime contracts were replaced.
- No high-frequency logs, service locator, new lifecycle, generic state-machine framework or duplicated transport-specific domain rules were added. Save-progress snapshots are only collected when the corresponding continuation needs polling.
- New production owner files are 116/371 physical lines; document/root/close hosts are 373/205/108 lines. New and substantively changed functions were reviewed against the 100-line guideline. The 278-line test file follows the same function guideline.
- Existing `EditorApplication.h` (707 lines) and `EditorPanels.cpp` (670 lines) remain over the file guideline: this change only adjusts declarations and read-only queries there. Their broader decomposition is deferred, consistent with the local-change migration rule.

## Build configuration

Both builds use the existing Windows x64 MSVC/Ninja presets with `BUILD_TESTING=ON` and Tracy OFF. Debug has RenderDoc ON; Release has RenderDoc OFF. No build option was changed for this refactor. The new focused test executable is declared only inside BUILD_TESTING and directly links `hyperion_content`.

```powershell
.\tools\Build.ps1 -Preset debug
.\tools\Build.ps1 -Preset release
```

Both final builds passed. Logs: `out/TransitionOwnership/DebugFinalBuild.log` and `ReleaseFinalBuild.log`.

## Tests

Tests use isolated `LOCALAPPDATA` directories under `out/TransitionOwnership`: BaselineLocalAppData, DebugLocalAppData and ReleaseLocalAppData. This avoids interference from existing local discovery records; the separately deferred discovery/test-isolation issue is unchanged. All desktop/GPU test runs were serialized.

The pre-change targeted Debug baseline passed 6/6, including the required content fixture: editor_state, editor_content_transition, automation_parity_regressions, automation_capability_parity and plugin_applications. Log: `BaselineTests.log`.

The related Debug suite passed 25/25 in 462.65 seconds. After the final close-reconfirmation boundary was added, the affected Debug subset was rerun and passed 11/11 in 139.16 seconds. Logs: `DebugTests.log`, `DebugFinalTests.log`.

```powershell
$RelatedTests = '^(editor_document_transitions|editor_state|editor_content_transition|editor_content_startup|editor_asset_editors|editor_acceptance|editor_multiselect|editor_selection_shortcuts|editor_reparent|editor_placement|editor_model_placement|scene_navigation|plugin_applications|automation_scene|automation_contracts|automation_connections|automation_capability_parity|automation_parity_regressions|automation_attachment|editor_asset_import|import_draft_automation)$'
$env:LOCALAPPDATA = 'F:\HyperionEngine\out\TransitionOwnership\DebugLocalAppData'
ctest --test-dir out/build/debug --output-on-failure -R $RelatedTests
ctest --test-dir out/build/debug --output-on-failure -R '^(editor_document_transitions|editor_state|editor_content_transition|editor_content_startup|editor_asset_editors|automation_capability_parity|automation_parity_regressions|automation_attachment|plugin_applications)$'
$env:LOCALAPPDATA = 'F:\HyperionEngine\out\TransitionOwnership\ReleaseLocalAppData'
ctest --test-dir out/build/release --output-on-failure -R $RelatedTests
```

The final Release suite passed 25/25 in 164.71 seconds; log `out/TransitionOwnership/ReleaseTests.log`.

The focused owner tests cover one-shot open/root actions, dirty/import/save admission, save rejection and asynchronous failure, save/asset-edit waiting, button/title-bar-equivalent cancellation, Save As cancellation, late completion after cancellation, close status/error/retry, overlap priority and close reconfirmation. Root candidates are intentionally opaque in this unit target; actual paths and admission are checked by the real service/GUI tests.

Real Editor and CLI/MCP regressions cover scene editing/history, root A/B/A and failure retention, cancellation during gated IO, API close failure followed by GUI cancel, native-close/root overlap, asset editing/import, discovery/describe/invocation, response-before-exit, plugin absence and shutdown. Existing acceptance expectations were retained; field accesses were migrated to owner operations/queries.

## Static validation

- Full `python tools/CheckStyle.py`: PASS, 1077 owned source files; formatting PASS.
- `python tools/CheckBoundaries.py`: PASS, 1038 source files across 39 modules.
- clang-tidy/clang-query semantic naming on all 18 changed translation units: PASS; log `out/TransitionOwnership/FinalNaming.log`.
- `git diff --check`: PASS.
- `openspec validate own-editor-document-transitions --strict`: PASS.
- Manual review covered semantic identity versus protocol text, single ownership of rules, named progress/action records, actual-input-driven polling, cancellation and content-service/provider boundaries. Static tools alone were not treated as behavioral proof.

These are targeted regressions for the affected contracts, not a claim that the complete repository test suite or every optional backend configuration was run.

## Authorized archive and delivery

On 2026-10-03, the user authorized archive followed by a local Git commit. The completed change was archived to `openspec/changes/archive/2026-10-03-own-editor-document-transitions/`, and its one added requirement with five scenarios was synchronized into `openspec/specs/editor-application-consolidation/spec.md`. All 108 main specifications passed `openspec validate --all --strict`; the archive-generated extra blank line at EOF was removed before the final diff check and scope freeze. Implementation sources were unchanged after the recorded builds and tests. No push was requested.
