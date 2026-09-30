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

### Requirement: Validation independent of scene enumeration
Single-node authoring and component validation SHALL check document identity, revision and live handles directly without enumerating all scene nodes. Batch preparation SHALL validate common document state once and validate every requested target before mutation. Existing error contracts and atomicity SHALL remain unchanged.

#### Scenario: Large scene single-node edit
- **WHEN** a valid single-node property or component edit is prepared against a large scene
- **THEN** document validation performs no full-scene node enumeration and preserves the existing edit result

#### Scenario: Invalid member in a batch
- **WHEN** a batch includes a stale, foreign or invalid target
- **THEN** the entire operation fails without partial mutation and without per-member full-scene enumeration

### Requirement: Shared ordered selection roots
Operations requiring top-level selected roots SHALL share an ordered hierarchy filter. It SHALL stop walking a node's ancestry once a selected ancestor covers it, preserve selected-root order and retain each operation's handle validation and transaction semantics.

#### Scenario: Fully selected deep hierarchy
- **WHEN** every node of a deep parent-child chain is selected for a roots-based operation
- **THEN** only the top root is returned and filtering does not traverse every ancestor for every descendant

#### Scenario: Mixed roots and invalid input
- **WHEN** independent branches and descendants are selected, or a caller supplies invalid handles
- **THEN** valid roots retain input order and invalid inputs follow that caller's existing atomic failure contract
