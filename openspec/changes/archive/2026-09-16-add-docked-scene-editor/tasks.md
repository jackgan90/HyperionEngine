## 1. GUI foundation

- [x] 1.1 Pin ImGui Docking and add engine-owned dock, image, menu, layout and inspection widgets.
- [x] 1.2 Extract reusable GuiRenderer with retained texture bindings and migrate DebugUI.
- [x] 1.3 Support explicit offscreen scene output and GUI sampling through RenderGraph.

## 2. Editor application

- [x] 2.1 Add standalone application lifecycle, mounted catalogs and menu-driven native scene opening.
- [x] 2.2 Build UE-inspired workspace, Outliner, Details, scene browser and saved layouts.
- [x] 2.3 Integrate viewport rendering and scoped reusable camera navigation with interruption handling.

## 3. Validation and delivery

- [x] 3.1 Add focused GUI contracts and editor interaction/render/lifecycle acceptance coverage.
- [x] 3.2 Build and run affected tests, inspect real editor captures and resolve failures.
- [x] 3.3 Complete style, naming, boundary and OpenSpec checks; document startup and limitations without committing.

Validation evidence: Debug and Release builds completed; focused GUI/RenderGraph/Deferred/D3D12 tests passed. The Debug editor and SceneViewer regression run passed 9/9 (including fixtures); Release GUI/editor coverage passed 6/6. Actual Sponza captures were inspected, camera movement/orbit/dolly and modal isolation passed, and D3D12 validation errors remained zero. Style, semantic naming, dependency boundaries, strict change validation and diff whitespace checks passed. No git commit was created.

## 4. Editor fly navigation refinement

- [x] 4.1 Add reusable fly navigation with RMB-gated translation and rotation at a fixed eye position; select it in Editor.
- [x] 4.2 Verify movement gating, rotation invariants, release/focus/capture interruptions and the actual Sponza editor workflow.
- [x] 4.3 Update control hints and documentation, rebuild Debug/Release, and complete scoped checks without committing.

Fly navigation validation: shared controller checks passed in Debug and Release, including all movement keys and aliases, RMB release/re-press, fixed-eye rotation, look direction, pitch limits and capture/focus/reset recovery. Debug SceneViewer moving-camera regression passed. Editor Sponza interaction acceptance passed in both configurations with movement gating, release stop, fixed-eye look, dolly and modal isolation all verified and zero D3D12 validation errors. Acceptance now waits for scene reopening to finish with a bounded timeout instead of relying on a fixed frame count. Formatting, changed-file naming, boundaries, strict OpenSpec validation and diff checks passed; no commit was created.

## 5. Editor wheel speed refinement

- [x] 5.1 Route RMB wheel to independent fly translation speed and preserve ordinary wheel dolly; show speed in the viewport toolbar and update control hints.
- [x] 5.2 Verify speed/pose invariants, actual translation distance, event ordering, interruption retention, bounds and Sponza editor input isolation.
- [x] 5.3 Update documentation, build Debug/Release, inspect the toolbar capture and complete scoped checks without committing.

Wheel speed validation: Debug and Release builds completed. Debug controller/editor/moving-camera regression passed 6/6 including fixtures; Release controller/editor coverage passed 4/4 including fixtures. Tests cover RMB wheel pose/lens invariants, positive/negative/fractional wheel input, matching movement distance, bare-wheel dolly with unchanged speed, button event ordering, capture/focus/reset retention, finite limits and modal isolation. The real Sponza capture shows Camera Speed 13.000 u/s and GPU validation errors are zero. Formatting, five changed translation units' semantic naming, module boundaries, strict OpenSpec validation and diff checks passed. No commit was created.

## 6. Independent quality audit

- [x] 6.1 Freeze the uncommitted review scope and obtain an independent review; reproduce confirmed findings.
- [x] 6.2 Preserve mixed legacy material output views for offscreen RGBA8 targets and add targeted graph/GPU regressions.
- [x] 6.3 Build and verify the fix in Debug/Release, complete scoped checks and obtain targeted independent re-review without committing.

Audit result: independent review found EDITOR-R1 (P2), a fixed-sRGB offscreen legacy pass rejecting valid linear material batches. The main agent reproduced the exception with a new native mixed-material GPU test, preserved per-batch RGBA8 views through scene targets and pass declaration, and kept non-RGBA8 restrictions unchanged. Final Debug graph/Deferred/GUI/editor/shared-material coverage passed 5/5; Release graph/GUI/editor/shared-material coverage passed 4/4. The original independent reviewer verified the fixed snapshot and independently reran graph/shared-material tests (2/2), with no new confirmed findings. Both builds, formatting, five affected translation units' naming, boundaries, strict OpenSpec validation and diff checks passed. Review records and hash manifests are under out/quality-audit/editor. No commit or archive was created.
