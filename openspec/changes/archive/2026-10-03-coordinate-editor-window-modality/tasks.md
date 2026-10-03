## 1. Platform window group

- [x] 1.1 Implement scoped membership, validation, lifetime cleanup and modal transitions.
- [x] 1.2 Implement private native input/stacking/visibility preservation and integrate FWindow lifecycle and Raise guards.

## 2. Editor integration

- [x] 2.1 Centralize main-modal state and register Asset Editor; remove per-host ownership reversal.
- [x] 2.2 Document registration, lifecycle, synchronization and unchanged automation/domain contracts.

## 3. Validation and review

- [x] 3.1 Cover multiple windows, dynamic membership, invalid registration, restoration, min/max/hidden/disabled state and shutdown in native tests.
- [x] 3.2 Verify real Editor close/cancel/save/discard and affected asset/content/plugin regressions.
- [x] 3.3 Run style/naming/boundary checks, builds and OpenSpec validation.
- [x] 3.4 Obtain independent reviewer follow-up, verify findings and complete bounded fixes; leave change unarchived and uncommitted.

## Validation record

- Review baseline: `f6cb0268de6796acba9e1ef4a962542ff859b7c8`; reviewed scope is this change's uncommitted Platform, Editor, tests and documentation. No archive, stage or commit performed.
- Debug `hyperion_editor` and `window_ownership_tests` builds passed. The 8-test run passed: `editor_asset_documents`, `editor_asset_editors`, `editor_content`, `editor_content_transition`, `window_ownership`, `plugin_runtime`, `plugin_applications`, `editor_close_modal` (including fixture dependencies).
- After lifecycle fixes, `window_ownership` and `editor_close_modal` passed again; the final owner-transfer fix passed `window_ownership` and the original independent reproduction. Close-modal acceptance checks native WM_CLOSE, ordering/enabled state, maximized Main, minimized auxiliary, Main minimize/cancel/restore, screenshots, unchanged drafts on cancel, persisted bytes on save and unchanged bytes on discard.
- Full formatting/path checks, changed-TU semantic naming, module boundaries, `git diff --check` and OpenSpec strict validation passed. Modified production C++ files remain within 500 physical lines; the new implementation is separated into group membership and private native state handling, without unrelated SDL input extraction.
- Independent reviewer confirmed MODAL-001 (multiple-host interference) and MODAL-002 (Main-minimize visibility) resolved. MODAL-003 (unregister/group destruction while minimized loses deferred visibility) and MODAL-004 (deferred visibility retains previous owner after reparenting) were independently reproduced by the main agent, repaired within Platform, added to regression coverage and independently re-reviewed. Final targeted review found no newly confirmed defect.
- Native behavior is implemented and tested on Windows. Native API failure coverage exercises an invalidated HWND after partial entry and validates rollback of surviving peers; arbitrary transient Win32 failures have not been exhaustively injected.
