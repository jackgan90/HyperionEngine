## 1. Live settings and viewport propagation

- [x] 1.1 Commit live depth through the shared settings service; unify scene/interaction/HUD consumers and preserve frozen culling.
- [x] 1.2 Freeze main render settings and pass the committed convention to all 3D asset preview frames, including later-opened/resumed windows.
- [x] 1.3 Update GUI/reflection labels and automation version/completion metadata while preserving persistence and revision contracts.

## 2. Regression coverage

- [x] 2.1 Add same-session/same-swapchain offscreen GPU switching coverage with pixels, shadows/contact/sky where applicable, and resource stability checks.
- [x] 2.2 Extend Editor GUI and automation acceptance for live switching, frozen culling/interactions, asset preview propagation, invalid/stale candidates, persistence and feature absence.

## 3. Documentation and validation

- [x] 3.1 Update current depth, diagnostics, automation coverage and verification documentation.
- [x] 3.2 Run OpenSpec validation, style/naming/boundary checks, Debug/Release builds and relevant CPU/GPU/Editor/automation regressions; resolve failures and record evidence.
- [x] 3.3 Review the final diff and task completion, leaving the change active and all code uncommitted.
