## Why

Starting an Outliner reparent drag currently inserts a temporary root drop zone above the object table, moving every object row. A permanent scene container should provide the same root destination with stable layout and clear scene ownership.

## What Changes

- Display a permanent, initially expanded Scene container as the first object-table row, with existing logical roots shown beneath it.
- Keep the Scene row available in empty scenes and filtered results, independently of drag state.
- Route drops on the Scene row through the existing atomic KeepWorld reparent operation with a null parent.
- Keep the presentation-only container outside object handles, selection, counts, copy/delete and transform operations; retain the native scene topology and persistence format.
- Add layout/interaction regression coverage and update Editor and automation documentation.
- Complete an independent quality audit and any confirmed in-scope repairs; leave this change active and uncommitted.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-hierarchy-drag-drop`: replace the temporary root zone with a permanent scene container and specify stable layout and presentation-only semantics.

## Impact

Editor Outliner rendering, the private reparent controller's root routing, existing Editor regression tests, and documentation. SceneEditing remains the sole mutation/history owner. Existing automation operation IDs, schemas, revisions, lifecycle, serialized scene records and plugin dependencies remain compatible; no new transport or runtime scene capability is required.
