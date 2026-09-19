## Context

Scene nodes persist affine local matrices, and Details editing already uses deterministic affine decomposition with raw upper-triangular shear. Editor owns selection and document history; Renderer owns reusable scene-view algorithms; Gui privately adapts ImGui. This document records the final implemented design after independent review.

## Goals / Non-Goals

Goals: reuse PRS math outside this Editor; show handles at the selected world origin; keep transforms defined for negative/zero scale; preserve document history and camera isolation; reclaim viewport space with icons and one toolbar row.

Non-goals: object highlighting, viewport object picking, snapping, multi-selection, a separate persisted rotation/scale representation, or new plugin lifecycle and backend paths.

## Decisions

1. **Runtime-owned CPU controller.** FTransformGizmo accepts camera, logical viewport bounds, local/parent matrices, mode and UI scale. It emits colored screen-space strokes and candidate local matrices without owning a scene or GUI objects. Gui clips the strokes into the existing image draw list; the normal GuiRenderer/RHI path composites them. This avoids binding reusable interaction math to Editor or native APIs.
2. **Stable logical size and reference basis.** Position uses world axes and converts world displacement through the parent inverse. Its center moves in the camera plane. Rotation and scale use decomposed local rotation axes transformed by the parent. Handles are independent of signed object scale; the drag basis is frozen through zero crossings. Rotation uses ray/ring-plane angles, with a projected tangent fallback at grazing angles. Unusable tangents and camera-aligned translation/scale axes do not start a drag.
3. **Affine and singular contracts.** Matrices stay authoritative. Scale changes are relative to the drag-start snapshot, with unit sensitivity for an initially zero diagonal component. Center scaling also scales raw shear entries; for nonzero initial scale components the full linear matrix scales proportionally, including factor zero and negative factors. Individual-axis scaling edits the corresponding decomposed scale component and preserves raw shear. A noninvertible parent disables Position handles. Nonfinite or invalid results are rejected. Fully collapsed matrices reuse the existing deterministic completed basis rather than attempting to persist an unobservable orientation.
4. **Editor-owned transaction.** Drag previews update the selected node without adding history entries. Finish restores the initial transform on the latest compatible node, preserving unrelated fields, then commits the preview through existing history once. Esc restores the exact initial matrix and preserves redo. Selection, revision, mode, viewport visibility/size, saving and focus interruptions finish the interaction. A conflicting external transform is not overwritten.
5. **Explicit pointer validity and capture.** The private ImGui adapter exposes position validity because its finite negative-max sentinel cannot be rejected with isfinite alone. Editor finishes a drag before processing an invalid position, retaining the last valid preview. Capture keeps dragging outside the viewport possible and prevents background window movement. Camera input is suspended during manipulation and the release frame.
6. **Compact theme-native controls.** Code-native vector translate, rotate and scale icons use normal editor colors, active blue backgrounds and hover tooltips. PRS and options remain on one row; the view selector uses remaining width when sufficient and stays accessible in options when omitted from a narrow row. Exposure, camera speed, initial-view and authored-camera actions remain available in that popup.

## Risks / Trade-offs

- Overlay handles are always visible rather than depth-tested scene geometry; picking uses their projected stroke geometry.
- Deterministic affine decomposition can redistribute reflection signs, and a fully zero matrix cannot retain an independent orientation. These are documented matrix-model constraints.
- Initial-zero recovery is an explicit exception to strictly multiplicative scaling. The zero component can be restored instead of remaining permanently zero.
- Tests do not exhaust all near-plane projections, arbitrary skew parents or extreme viewport widths. Focus-loss regression uses actual GUI processing of Platform-format focus events, not a physical Alt+Tab replay.

## Validation and Migration

No scene format migration or plugin contract change is required. CPU tests cover PRS, mirror/zero recovery, singular parents and complete sheared uniform-scale matrices. Editor acceptance exercises all three toolbar modes, live preview, one-entry history, cancel/redo preservation, unrelated-property revision changes and focus-loss undo/redo.

Independent review found and closed EDITOR-001 (invalid pointer on focus loss) and MATH-01 (raw shear omitted from uniform scale). Debug build and five final targeted CTests passed with final GPU validation errors zero; formatting, boundaries and diff checks passed. Detailed local evidence is in out/PrsQualityAudit and is not part of the source commit. Release and a full-repository test run were not performed for this change.
