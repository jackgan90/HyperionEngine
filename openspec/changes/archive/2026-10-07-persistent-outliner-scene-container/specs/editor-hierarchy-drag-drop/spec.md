## MODIFIED Requirements

### Requirement: Discoverable targets and cancellable gestures
Editor SHALL provide a permanent Scene container as the first Outliner object-table row and use that row as the explicit scene-root drop zone, alongside node targets in tree and search results, validity feedback, hover expansion and edge scrolling. The Scene row SHALL remain present outside dragging, in empty scenes and while filtering; starting, cancelling or ending a drag SHALL NOT insert or remove layout rows. Only delivery onto a valid target SHALL mutate the scene. Esc, focus loss, right mouse interruption, invalidated document/revision/selection and dropping elsewhere SHALL cancel without mutation. Drag snapshots SHALL own their handles and SHALL NOT be limited by embedding all handles in the GUI payload.

#### Scenario: Detach to root
- **WHEN** selected nodes are dropped onto the Scene container
- **THEN** selected roots have no parent and retain their world transforms through the existing shared null-parent KeepWorld transaction

#### Scenario: Invalid or cancelled drop
- **WHEN** a drag is cancelled or targets itself, a descendant, a stale handle or an invalid parent transform
- **THEN** feedback explains the rejection and no scene, history, revision or dirty-state mutation occurs

#### Scenario: Navigate a large hierarchy
- **WHEN** a node is dragged over a collapsed row or near the Outliner edge
- **THEN** the row opens after a hover delay or the list scrolls, allowing nested targets to be reached

#### Scenario: Stable drag layout
- **WHEN** a drag starts, is cancelled or completes without a topology/filter/expansion change
- **THEN** the Scene and object rows retain their positions and heights and no temporary root action row appears above the table

## ADDED Requirements

### Requirement: Presentation-only scene container
The Outliner Scene item SHALL represent the current scene document using a separate GUI identity from logical node items and display the scene filename without its extension, or `Untitled Scene` when no path exists, with Type `Scene`. Existing logical roots and filtered objects SHALL be displayed beneath the initially expanded container. Changing to a nonempty search SHALL expand the container once. Users SHALL be able to collapse/expand it without changing scene data, history, revision, dirty state or selection. The container SHALL NOT have a scene handle, enter object counts or object-row selection ranges, originate object drags, or participate in object select-all, transform, copy, delete or persistence operations.

#### Scenario: Scene ownership presentation
- **WHEN** an existing scene opens in Editor
- **THEN** a permanent Scene row displays its document label above the existing root objects without changing their authored parents or creating a logical node

#### Scenario: Search and empty scene
- **WHEN** a scene has no nodes or the search matches no objects
- **THEN** the Scene row remains visible, and changing a nonempty search expands it independently of previous collapse

#### Scenario: Ordinary object operations
- **WHEN** users click or collapse the Scene row, or select all, range-select, copy, delete or transform objects
- **THEN** the container remains outside object selection and commands and object counts include only logical nodes

#### Scenario: Document replacement
- **WHEN** the current scene document is replaced
- **THEN** the new container has its own document-specific GUI identity and defaults to expanded without reviving an old reparent gesture
