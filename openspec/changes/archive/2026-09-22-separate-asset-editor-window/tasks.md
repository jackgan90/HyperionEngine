## 1. Native asset host

- [x] 1.1 Add a plugin-owned secondary window with independent GUI/layout, swapchain, render submission and safe partial-initialization cleanup.
- [x] 1.2 Remove scene/asset mutual exclusion and give the asset host its own preview and property panels, retaining asset tab identity and history.

## 2. Input and lifecycle

- [x] 2.1 Route commands, cameras and text input by native window; preserve shared application scale and per-window pixel sizing.
- [x] 2.2 Support close/save/discard/cancel, recreation, independent minimization, global exit and content-root switching without losing saves or scene edits.

## 3. Verification and documentation

- [x] 3.1 Adapt existing asset input acceptance and add simultaneous rendering, input isolation and native-window lifecycle regressions.
- [x] 3.2 Run Debug/Release builds and affected asset, GUI, Editor, window and plugin tests; fix failures.
- [x] 3.3 Run style/naming/boundary/OpenSpec checks, update documentation and record evidence; leave all changes uncommitted.

## 4. Desktop ownership regression

- [x] 4.1 Make the asset window a non-modal owned top-level window, preserving input focus in the clicked window and native application-group stacking.
- [x] 4.2 Cover visible native activation, stacking, owner minimize/restore, asset minimize/restore, close isolation and recreation; rerun affected Debug/Release acceptance.
- [x] 4.3 Update the window behavior documentation and verification evidence; leave changes uncommitted.

## 5. Content Browser double-click regression

- [x] 5.1 Reproduce and fix opening an unselected asset on its first double-click, including another asset while the asset window is open.
- [x] 5.2 Add input regressions, run affected Debug/Release validation and record evidence without committing.

## 6. Independent quality audit

- [x] 6.1 Guard main-window save shortcuts with scene readiness and modal state, contain save failures, and cover unavailable/failed saves.
- [x] 6.2 Validate the shortcut and pending-edit window protections and obtain targeted independent re-review without committing.
