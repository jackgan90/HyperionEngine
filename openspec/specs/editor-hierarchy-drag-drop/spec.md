# editor-hierarchy-drag-drop Specification

## Purpose
Define Outliner-only hierarchy drag gestures and shared atomic KeepWorld reparenting, preserving ordered selection, internal hierarchy, history and scene persistence.
## Requirements
### Requirement: Selection-aware hierarchy gestures
Editor SHALL remove the entire Details Hierarchy section and expose fixed-KeepWorld reparenting only by dragging Outliner rows onto Outliner nodes or the scene-root drop zone. Dragging an already selected row SHALL retain the complete ordered selection and primary. Viewport selection SHALL remain synchronized with Outliner, but viewport gestures SHALL NOT start reparenting. Ordinary click and Ctrl-toggle semantics, tree expansion, Gizmo, camera navigation and placement SHALL remain usable.

#### Scenario: Drag a viewport-selected group
- **WHEN** multiple nodes are selected through Viewport or Outliner and a selected Outliner row is dragged to another Outliner node
- **THEN** the selected roots become children of that target without changing selection or world affine transforms

#### Scenario: Viewport gestures do not reparent
- **WHEN** a selected model or light is dragged from Viewport toward Outliner, with or without a Gizmo handle under the pointer
- **THEN** no hierarchy drag payload is created and no parent changes occur; viewport picking and Gizmo behavior retain their existing semantics

#### Scenario: Reparent a viewport-selected light
- **WHEN** the user selects a light in Viewport, then drags its selected Outliner row onto another Outliner node
- **THEN** the light becomes a child of that target while retaining its world transform and selection

#### Scenario: Preserve selected internal hierarchy
- **WHEN** selected parent A and its selected child B are dragged to C
- **THEN** A becomes a child of C and B remains a child of A, preserving both world transforms

#### Scenario: Distinguish click and drag
- **WHEN** the user presses an already selected row and either releases without dragging or exceeds the drag threshold
- **THEN** the first gesture performs the original click/toggle action and the second drags the original selection without reducing or toggling it

### Requirement: Discoverable targets and cancellable gestures
Editor SHALL provide an explicit scene-root drop zone, targets in both tree and search results, validity feedback, hover expansion and edge scrolling. Only delivery onto a valid target SHALL mutate the scene. Esc, focus loss, right mouse interruption, invalidated document/revision/selection and dropping elsewhere SHALL cancel without mutation. Drag snapshots SHALL own their handles and SHALL NOT be limited by embedding all handles in the GUI payload.

#### Scenario: Detach to root
- **WHEN** selected nodes are dropped onto the scene-root zone
- **THEN** selected roots have no parent and retain their world transforms

#### Scenario: Invalid or cancelled drop
- **WHEN** a drag is cancelled or targets itself, a descendant, a stale handle or an invalid parent transform
- **THEN** feedback explains the rejection and no scene, history, revision or dirty-state mutation occurs

#### Scenario: Navigate a large hierarchy
- **WHEN** a node is dragged over a collapsed row or near the Outliner edge
- **THEN** the row opens after a hover delay or the list scrolls, allowing nested targets to be reached

### Requirement: Shared atomic batch reparenting
SceneEditing SHALL expose reflected fixed-KeepWorld batch reparenting to GUI and Automation with document/revision guards, explicit handles and optional parent. It SHALL validate every handle, normalize selected roots, preserve complete affine transforms, and commit all changed roots as one history entry. Already-parented roots SHALL be no-ops. Existing scene.node.reparent and KeepLocal contracts SHALL remain compatible.

#### Scenario: Atomic undo and persistence
- **WHEN** a multi-node reparent succeeds, is undone, redone, saved and reopened
- **THEN** one undo/redo restores the entire batch, selection is retained during reparent/history, and saved parent/local transforms reconstruct the same world transforms

#### Scenario: Reject an entire batch
- **WHEN** any input is stale, foreign, cyclic or produces an invalid transform
- **THEN** the whole request fails without a partial edit or history entry

#### Scenario: No-op and discovery
- **WHEN** an agent discovers and describes scene.nodes.reparent, then invokes it with nodes already under the requested parent
- **THEN** the schema explains fixed KeepWorld and selected-root semantics and invocation leaves revision, history and dirty state unchanged
