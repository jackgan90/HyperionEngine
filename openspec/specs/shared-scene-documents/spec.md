# shared-scene-documents Specification

## Purpose
Define the UI-independent scene document authority shared by human and agent operations, including transactions, history, save points, interaction ownership and document invalidation.
## Requirements

### Requirement: UI-independent scene document authority
Scene editing SHALL provide one Main-owned service for document identity, transactions, revision validation, history and save points. Editor GUI and automation SHALL use the same instance. CPU document services SHALL remain independent of GUI, Renderer and RHI; renderer-specific resource effects SHALL use an explicit target interface.

#### Scenario: Existing GUI editing
- **WHEN** existing property, structural, settings or grouped transform edits are committed, undone and redone
- **THEN** the shared service preserves their established atomicity, history grouping and selection-restoration behavior

### Requirement: Explicit save point completion
Saving SHALL capture authored state and its document/state identity, complete asynchronously, and mark only that captured state as saved. Save failure and document replacement SHALL not incorrectly clear dirty state.

#### Scenario: Later edit while saving
- **WHEN** another edit is committed after the save snapshot
- **THEN** successful save completion leaves that later edit dirty

### Requirement: Interaction ownership and document invalidation
The service SHALL distinguish active GUI interactions from completed transactions and reject conflicting agent writes. Loading, closing or replacing the content environment SHALL invalidate old document references and preserve existing dirty/busy policies.

#### Scenario: Replace the current scene
- **WHEN** a new scene replaces the current document
- **THEN** old document references fail even if object names or slot numbers are reused

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
