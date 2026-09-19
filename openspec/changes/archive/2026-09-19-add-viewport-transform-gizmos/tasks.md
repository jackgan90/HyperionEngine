## 1. Reusable PRS controller

- [x] 1.1 Add engine-owned transform modes, handles and screen-space geometry to Runtime/Renderer without GUI or concrete-plugin dependencies.
- [x] 1.2 Implement free/world-axis translation, local-axis rotation, axis scaling and center scaling at the selected object's origin.
- [x] 1.3 Define mirror, initial-zero recovery, singular-parent rejection, frozen drag basis and invalid-input behavior.
- [x] 1.4 Cover affine shear during center scaling with complete matrix assertions for positive, zero and negative factors.

## 2. Editor and GUI integration

- [x] 2.1 Integrate Outliner-selected objects and clipped overlays without object highlighting or viewport object picking.
- [x] 2.2 Add image pointer capture and explicit pointer-position validity; isolate camera input during manipulation.
- [x] 2.3 Preserve one-command Undo/Redo, exact Esc cancellation, save/selection/revision completion and unrelated property updates.
- [x] 2.4 Add theme-native PRS vector icons, hover descriptions and a compact toolbar with retained view/options actions.
- [x] 2.5 Document transform conventions, zero recovery, toolbar behavior and validation entry points in docs/Editor.md.

## 3. Verification and review

- [x] 3.1 Build Debug and run transform_gizmo, gui_input_and_data, gui_docking, editor_acceptance and gui_scale_acceptance after fixes.
- [x] 3.2 Add actual GUI event focus-loss regression for all three modes with last-preview preservation and Undo/Redo assertions.
- [x] 3.3 Run independent math and Editor/GUI reviews, reproduce confirmed findings, implement bounded fixes and obtain focused re-review closure.
- [x] 3.4 Verify formatting, module boundaries, diff whitespace, targeted clang-tidy and the reviewed source hash snapshot.

## 4. OpenSpec closeout

- [x] 4.1 Reconstruct the missing change record from the final implementation and evidence, explicitly identifying its retrospective status.
- [x] 4.2 Specify reusable PRS contracts and the compact toolbar/navigation presentation delta for synchronization before archive.
