## Context

Selection and editor history currently contain one handle. Renderer outlines already accept object groups, Reflection supplies editable display projections, and Scene uses staged FMutation commits. Built-in components are unique by type. Existing single-object transforms preserve affine shear and recover zero scale.

## Goals / Non-Goals

**Goals:** Ctrl-toggle ordered selection and primary identity; common component inspection with exact mixed values; absolute local Details assignments; primary-centered gizmo transforms; atomic batch changes and one history entry per gesture; multi-delete with restored selection; regression validation.

**Non-Goals:** Box/range selection, custom pivots, world/local mode switching, multi-component-instance matching, asset geometry replacement, batch renaming or component add/remove UI, archive and git commit.

## Decisions

1. Editor owns an ordered selection value with unique generation-safe handles and the last-added primary. Plain click replaces, Ctrl-click toggles, removing the primary chooses the last remaining handle. Plain viewport miss clears; Ctrl-miss and unavailable queries preserve. Capture Ctrl on press. Existing gizmo/navigation/dialog priority remains. Search and collapse do not alter selection. Selection does not dirty or serialize.
2. Every selected Outliner row and existing light marker is highlighted. Outline requests carry one primitive group per selected object and the matching publication token; existing Union/PerObject options remain. Selected non-renderable objects receive small projected origin markers. Only the primary owns the gizmo.
3. Reflection aggregates drafts of the same display type with exact typed equality. GUI emits explicit leaf edits, including a submission equal to the primary value. Apply these edits to each object's own draft; never copy the primary's complete component. Vector axes have independent mixed flags. Optional presence uses a mixed state. Collections require corresponding stable identities (mesh primitive IDs plus matching asset context); unmatched structures remain visibly mixed and read-only. Multiple instances of a non-unique type are not guessed.
4. Details uses absolute local PRS assignments, including numeric dragging. Ordinary properties apply the same leaf value to every selected object. Object Enabled is batch editable; name, add/remove component and single-target camera/light actions remain single-selection actions to avoid accidental structural changes.
5. Gizmo freezes primary local/parent/world matrices and top-level selected roots at drag start. Translation uses a common world displacement. Rotation derives the primary's rotation delta without inverting its possibly singular stretch. Scale derives the world affine delta from its initial and target matrices when defined. Apply the delta only to selected roots; descendants inherit once. Thus the primary's final world result matches its existing single edit, including under nonuniform parents; resulting shear is preserved. All previews are snapshot-relative. Undefined initial scale ratios or required singular parent inverses disable the corresponding group operation with a reason. Details and single selection retain zero-scale recovery; crossing zero within a valid gesture remains supported.
6. Scene extends staged mutation to batches and validates final hierarchy/world values before one commit. SceneInstance preflights all resource restrictions and prepares side effects before authoritative changes. Editor batch history records per-object before/after values, document state and structural selection snapshots. No partially successful edit is published. Undo/redo batches are atomic. Gizmo cancellation restores only owned transforms; unrelated property updates survive. Invalid pointer/focus/selection/save interruption follows existing finish behavior.
7. Multi-delete reduces selected objects to non-overlapping roots, captures parent-first subtrees, deletes in one transaction, and clears selection. Undo restores all deleted objects and original ordered selection; remap recreated handle generations throughout later history, settings and selection snapshots. Redo deletes all roots and clears selection. Ordinary property undo preserves current selection.
8. Runtime Reflection/Gui own reusable inspection mechanics, Scene owns CPU mutation, Renderer owns reusable transform computation and publication integration, and Editor owns selection/history/UI policy. No concrete plugin dependency or Runtime/Application lifecycle is introduced. Existing plugin absence/shutdown paths remain.

## Risks / Trade-offs

- Mixed state becoming authored data or unrelated fields being overwritten -> keep aggregation separate and record explicit field paths, with nested/vector/optional tests.
- Parent-child double transformation or sequential validation failures -> selected-root transforms and final-state staged validation.
- Zero/singular matrix inversion -> operation-specific preflight and controlled rejection, including undo/cancel paths.
- Nonuniform parent produces non-rigid rotation -> accepted primary single-edit compatibility, full matrices retained.
- Different model section meanings -> stable identity matching or read-only mixed collection, never index-only broadcasting.
- Async scene/resource changes -> revision guards and target snapshots; do not overwrite external transform conflicts.

## Migration Plan

No scene schema or persisted preference migration. Single-selection behavior remains the one-element case. Build and validate CPU tests plus real editor interactions; leave the completed change active and all changes uncommitted.

## Open Questions

None blocking: selection toggles, primary policy, absolute Details versus relative gizmo, affine compatibility and undefined-ratio/collection limits were confirmed by the user.
