## Context

Editor uses FSceneEditDocument for shared selection, transactions and history. Single-node CommitReparent already stores affine local matrices. Scene EditNodes stages all candidates and validates cycles/world transforms before authoritative writes. FGui owns ImGui adapters and copies small drag payloads. Outliner tree clicks currently fire on press; viewport picking completes on release and cancels on movement.

## Goals / Non-Goals

**Goals:** Remove Details Hierarchy; support Outliner-only sources and parents/root targets, synchronized viewport selection, ordered multi-selection, selected-root normalization, KeepWorld, atomic undo/redo, save/reopen and equivalent Automation.

**Non-Goals:** Viewport drag sources or drop targets for reparenting, sibling reordering, scene/model asset topology changes, new transform representation, removal of the existing single-node KeepLocal API, plugin composition changes, archive or commit.

## Decisions

1. Add a shared batch candidate builder and commit operation in SceneEditing. Requests contain document/revision, explicit handles and optional parent. Validate every input before reducing ancestors; selected ancestors cover descendants. Compute inverse(parent.World) * node.World from one pre-edit snapshot and submit one CommitEdits. Do not loop single-node transactions. Retain existing single-node API semantics.
2. Register scene.nodes.reparent with reflected request/result contracts and fixed KeepWorld. GUI uses the same authoring entry; transports gain no domain logic. A read-only candidate builder supplies target feedback, while final commit repeats validation.
3. Editor owns a transient gesture containing token, document/revision, ordered handles, press position/source and pending drop. Payload contains only its token, avoiding the existing 4 KiB limit and borrowed pointers. Cancel on Esc, focus loss, right mouse, document/revision/selection invalidation or unavailable UI. End drag occupancy before RequireIdle, then commit after Outliner traversal.
4. Distinguish click and drag before reducing selection. Defer clicks on selected rows until release; start drags at a GUI-scaled distance threshold. Unselected Outliner rows select before dragging. Only Outliner rows can start reparent gestures. Viewport picking continues to update the same selection, which users then drag from Outliner. Remove the viewport reparent route, its duplicate hit query and source flag. Use engine-owned GUI helpers for explicit drag sources, row target feedback/hover opening and scrolling; third-party APIs remain private adapters.
5. Show a scene-root drop zone during a node drag; null parent detaches selected roots. Both tree and filtered rows accept drops. Highlight valid/invalid targets, explain errors and inherited disabled state, auto-expand on hover and after success, and scroll near list edges. Only delivery commits. Preserve selection and primary.
6. Keep world affine matrices (including shear and negative/nonuniform scale), rather than decomposing/recomposing TRS. Skip already-parented roots; all-no-op does not change history, revision or dirty. Effective enabled state continues following parent inheritance.

## Risks / Trade-offs

- Click timing changes → preserve Ctrl-toggle and plain-click results; exercise real GUI input for both filtered and tree rows.
- Viewport objects and Gizmo handles overlap, particularly light markers → reserve reparent initiation for Outliner; validate viewport selection followed by Outliner dragging and absence of viewport reparent payloads.
- Mutation while traversing tree → queue delivery and commit after traversal; never retain node pointers.
- Singular/nonfinite transforms or stale state → shared validation and atomic Scene batch rejection; assert no partial history/revision changes.
- Drag occupies document busy state → explicitly finish only this gesture before shared invocation and refresh remaining busy guards.
- Parent/child selections → preserve internal hierarchy as explicitly confirmed by user, while retaining the full original ordered selection.

## Migration Plan

No asset migration. Preserve existing operation IDs and single-node schemas. Add batch schema and update Editor/Automation documentation; retain delta specs unarchived. Rollback is the scoped source change, with persisted scene parent/local contracts unchanged.

## Open Questions

None. User confirmed selected-root behavior and revised acceptance to require Outliner-only reparenting after observing ambiguity between viewport dragging and Gizmo handles.
