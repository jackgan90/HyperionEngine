# Verification

Validated on Windows x64 with the Debug preset on 2026-09-27.

## Build

- `tools/Build.ps1 -Preset debug -Target hyperion_editor` passed.
- Debug targets `automation_scene_tests`, `clipboard_tests`, `scene_navigation_tests`, `gui_tests` and `editor_state_tests` built successfully.
- Final Editor relink after reflected field descriptions and acceptance startup guards passed.

## Tests

The targeted CTest run passed 12/12 tests (83.02 seconds):

- `automation_scene`
- `editor_state`
- `platform_clipboard`
- `model_fixtures` and `native_model_fixtures` (required fixtures)
- `scene_navigation`
- `plugin_applications`
- `gui_input_and_data`
- `automation_capability_parity`
- `editor_multiselect`
- `editor_clipboard`
- `editor_reparent`

After the final acceptance startup changes, `plugin_applications` and `editor_clipboard` passed again, 2/2 (18.83 seconds). The last clipboard report, log and capture are in `out/editor-tests/clipboard-p1m55vm3/`; CTest output is in `out/build/debug/Testing/Temporary/LastTest.log`.

Domain tests cover immutable authored state and material snapshots; ancestor/child deduplication; ordered selection and primary restoration; numbered names; zero/negative scale and disabled nodes; internal model-source and extension references; deleted source and external parent behavior; same-ID parent replacement; save points; clipboard I/O failures; ordinary text/token mismatch; reset/invalidation; node and byte budgets; unsupported components; failed batch admission/rebind/redo; and registered automation discovery/invocation using the shared clipboard.

Renderer tests verify shared model resources, paste/undo/redo, saved/reopened scene equality and document reset. Native clipboard tests verify Windows custom format plus plain text, foreign Win32 text replacement, clearing and owner-thread enforcement. Editor acceptance injects real GUI input to verify subtree paste, repeat suppression, undo/redo independent of the clipboard, Content Browser isolation and text-widget copy/paste ownership. Plugin acceptance includes unavailable Editor dependencies and incomplete clipboard exercise failures. Live automation parity walks and describes the registered operations and reflected schemas.

## Static checks

- `python tools/CheckStyle.py`: filenames/include casing and formatting passed (910 owned source files).
- `CheckStyle.check_naming`: all 26 changed C++ translation units checked (initial 25, then the final EditorHost and SceneClipboard units); clang-tidy and boolean declaration checks passed.
- `python tools/CheckBoundaries.py`: 874 source files and 38 modules passed.
- `git diff --check`: passed.
- `openspec validate add-editor-object-clipboard --strict`: passed.

## Delivery state and scope

All implementation tasks are complete. The change is intentionally unarchived and uncommitted. No persistent scene schema, existing operation ID or transport-specific domain branch was introduced.

The first version supports the Windows provider within one Editor instance and one open scene document. Cross-document/process/restart paste is outside this change. Other platforms report the typed clipboard as unavailable. Validation used Debug; a full repository test sweep and other build configurations were not run.

## Placement focus acceptance fix

User acceptance found that immediately copying after dropping an object did nothing. A new real GUI drag/drop regression reproduced the failure before the fix: `Ctrl+C after placement did not copy the selected object; placement focus=1`. Selection had moved to the new object, while keyboard focus remained in Place Object and the scene shortcut guard correctly rejected it.

The successful GUI drop path now focuses Viewport immediately after committing placement. Shared/automation placement and cancelled/failed delivery do not receive this focus change. Shortcut scope and text ownership guards remain unchanged.

The placement exercise now copies and pastes each of the five shapes and three lights directly after delivery, without clicking or focusing the viewport in the test. It verifies distinct identities, numeric names, authored property equality, primary-light preservation, undo selection restoration and redo preservation across cancelled drags. The existing preview, cancellation, marker picking, resource failure and save/reload checks still run.

- Fix-before-test evidence: `editor_placement` failed at the first copy after Cube delivery (3.28 seconds).
- After the fix: `editor_clipboard` passed (18.57 seconds), retaining text and unrelated-panel isolation coverage.
- Final extended `editor_placement` passed (12.23 seconds); artifacts: `out/editor-tests/placement-pbmvfrih/`.
- Debug Editor build passed; formatting, semantic naming for affected translation units, dependency boundaries, strict OpenSpec validation and whitespace checks passed.
- Follow-up remains unarchived and uncommitted.

## Independent quality audit

The independent reviewer audited the complete clipboard development and placement-focus follow-up against baseline `50b6d44b6421fd8135cd9d422e0e88b915607690`, including modified and new files. The initial 61-file SHA256 snapshot is `out/acceptance/ClipboardAudit/InitialManifest.json`. Review covered clipboard ownership and replacement, hierarchy and selection, immutable authored state, resource refresh, atomic history, GUI focus and input guards, platform lifetime, automation contracts and acceptance coverage.

### CLIP-01 (P2): stale model resources after removing all live consumers

- Trigger: copy a model, delete every scene consumer of its asset, save the model or a default material, refresh assets, then paste. The clipboard survives deletion, but live refresh skipped the unused model cache. Rebind and batch creation reused its stale data.
- Main-agent verification: independently traced capture, generation invalidation, renderer refresh and batch admission, then reproduced the defect with a real Renderer/SceneEditing regression before changing production code. `scene_runtime_instance` failed in 6.10 seconds at `Data->Materials.front()->Asset->Name == "Saved while clipboard is the only consumer"`.
- Fix: retain an invalidation flag on unused model cache entries and resolve the current native resource graph during clipboard/history rebind. Clear the flag only after successful loading. Unused entries are not eagerly loaded; failed rebind retains invalidation and rejects the paste before scene/history mutation.
- Regression: saved default material and geometry changes are visible after copy/delete/refresh/paste; missing dependencies reject two consecutive paste attempts without changing scene revision, history cursor or selection.
- Disposition: confirmed and fixed. The original independent reviewer rechecked the implementation and direct callers, verified all 64 hashes in `out/acceptance/ClipboardAudit/FixedManifest.json`, and closed CLIP-01 with no additional confirmed defects. Source code stayed frozen during both review passes.

### Audit validation

- Debug builds passed for `scene_instance_tests`, `hyperion_editor` and `scene_navigation_tests`; logs: `out/acceptance/ClipboardAudit/{RegressionBuild,FixBuild,EditorBuild,NavigationBuild}.log`.
- `scene_runtime_instance` passed after the fix (8.42 seconds).
- `scene_navigation`, `editor_clipboard`, `editor_placement` and their two required fixtures passed (5/5, 31.47 seconds); raw output: `out/acceptance/ClipboardAudit/IntegrationTests.log`. Latest GUI artifacts: `out/editor-tests/clipboard-g1usdf00/` and `out/editor-tests/placement-cabldtyh/`.
- The reviewer also ran `automation_scene_tests.exe` successfully during initial review. The reviewer did not repeat GPU tests and independently inspected the final integration log.
- Full source formatting, filenames/includes, dependency boundaries, semantic naming for the two audit-modified translation units, whitespace and strict OpenSpec validation passed.

Validation boundary: in an empty scene, `Await(bReady)` does not guarantee asynchronous refresh publication has finished. Dynamic regressions exercise invalidation after refresh starts; preservation of the flag after refresh completes was verified by independent review of the publication path. This audit used Debug and targeted tests, not all build configurations or a full repository test sweep. No confirmed findings remain open. The change remains unarchived and uncommitted.
