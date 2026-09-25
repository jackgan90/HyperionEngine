## ADDED Requirements

### Requirement: Undoable structural authoring
The shared document SHALL support resource-preserving duplication of the primary node and deletion preserving child world transforms. Both actions SHALL validate before mutation, create one history transaction, restore settings and selection through undo/redo, and retain stable asset resource sharing. Editor and Automation SHALL use the same methods.

#### Scenario: Duplicate and replay history
- **WHEN** a model is duplicated, undone and redone
- **THEN** the new node shares its source resources, has a distinct identity, and selection/history refer to valid recreated handles

#### Scenario: Preserve children on deletion
- **WHEN** a parent is deleted with keep-children semantics and then undone
- **THEN** children keep their world transforms during deletion and regain their original parent/local transforms on undo, with scene selections restored

#### Scenario: Reject invalid topology
- **WHEN** a requested structural change cannot preserve valid transforms or references
- **THEN** the document, selection, dirty state and history remain unchanged
