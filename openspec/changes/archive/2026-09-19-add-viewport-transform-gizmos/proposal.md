## Why

The editor needs direct manipulation of the Outliner-selected object's position, rotation and scale inside the rendered viewport. Text controls and multiple toolbar rows consume scene space; recognizable icons and hover descriptions keep the existing actions accessible in a compact layout.

This change record was completed from the implemented and independently reviewed work before archival; it does not represent a proposal reviewed before implementation.

## What Changes

- Add reusable Runtime transform gizmo geometry, hit testing and drag computation, with world-axis/free translation, local-axis rotation and signed axis/center scaling.
- Define mirrored, zero, singular-parent and invalid-input behavior without introducing a second persisted transform representation.
- Integrate Outliner selection, clipped viewport overlays, exclusive camera input and one-command editing transactions, including cancellation and focus loss.
- Replace PRS labels with theme-consistent vector icons and hover tooltips. Keep a single toolbar row and expose camera, exposure and view actions in a viewport options popup.
- Add CPU transform regression and actual Editor GUI/GPU acceptance, including independently reproduced focus-loss and sheared-uniform-scale defects and their fixes.

## Capabilities

### New Capabilities
- `scene-transform-gizmos`: Reusable PRS manipulation, defined degeneracy handling and Editor transaction integration.

### Modified Capabilities
- `scene-editor`: Compact transform toolbar and camera-speed display within viewport options.

## Impact

Runtime/Renderer gains the CPU-only FTransformGizmo controller; Runtime/Gui exposes engine-owned icon, overlay, pointer-validity and capture APIs. The Editor plugin owns scene/history integration and acceptance orchestration. Existing module targets, scene matrix serialization, plugin lifecycles and rendering paths remain in use. No third-party dependency, object highlight or viewport object picking is added.
