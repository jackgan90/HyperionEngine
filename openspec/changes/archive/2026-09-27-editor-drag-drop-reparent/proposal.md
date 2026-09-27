## Why

Changing parents through two Details dropdowns is cumbersome and cannot operate on a selection. Scene hierarchy editing should support direct drag and drop while preserving the visible world transform and existing undo semantics.

## What Changes

- Remove the complete Details Hierarchy section.
- Drag selected nodes only from Outliner rows onto an Outliner parent or an explicit scene-root drop zone. Viewport selection remains synchronized with Outliner; viewport gestures retain picking and Gizmo behavior without starting reparenting.
- Preserve ordered selection, primary identity, internal selected hierarchy and world affine transforms; submit one atomic history entry per drop.
- Provide valid/invalid target feedback, hover expansion, scrolling, cancellation and stale-document protection.
- Add a reflected fixed-KeepWorld batch operation shared by GUI and Automation, preserving the existing single-node API.

## Capabilities

### New Capabilities
- `editor-hierarchy-drag-drop`: Selection-aware hierarchy gestures and shared atomic batch reparenting.

### Modified Capabilities
- `editor-render-diagnostics`: Replace the requirement to expose both reparent modes in Editor with fixed-KeepWorld drag and drop.

## Impact

Runtime SceneEditing, Gui adapters, Editor interaction/picking, Automation registration, documentation and CPU/GUI/Editor acceptance tests. Reuse Scene batch mutation and existing Renderer publication. No new dependencies or plugin lifecycle model. No archive or Git commit in this delivery.
