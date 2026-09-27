## ADDED Requirements

### Requirement: Immutable selected subtree copy
The system SHALL capture ordered selected nodes and their descendants once, deduplicating selected ancestors and descendants. It SHALL preserve all supported authored component values and isolate mutable state, sharing immutable asset resources. Copy SHALL NOT dirty the scene, create history or truncate redo. Empty copy SHALL retain the previous clipboard. Unsupported components and exceeded budgets SHALL fail without partial capture.

#### Scenario: Source changes after copy
- **WHEN** a selected subtree is copied and original nodes are edited or deleted
- **THEN** paste uses the captured values and does not require the original copied nodes to survive

#### Scenario: Parent and child selected
- **WHEN** both a node and its descendant are selected before copy
- **THEN** the subtree is captured once and pasted selection preserves the original explicit order and primary mapping

### Requirement: Authoritative typed system clipboard
Object paste SHALL require the current system clipboard's versioned object token and matching live document snapshot. Text, image, file or other clipboard replacement SHALL disable the old object paste. Clipboard read/write errors SHALL be reported without cached fallback. Text widgets SHALL retain ordinary text copy/paste.

#### Scenario: Text replaces objects
- **WHEN** objects are copied and then ordinary text is copied in Editor or another application
- **THEN** scene paste creates no objects and does not use the previous local snapshot

#### Scenario: Clipboard token mismatch
- **WHEN** the token is malformed, replaced, from another process or from an expired document
- **THEN** paste fails without changing scene, selection, history or dirty state

### Requirement: Atomic identity and hierarchy preserving paste
Each paste SHALL allocate fresh node identities and scene-wide unique numbered names, preserve authored values and Local matrices, remap internal node references, and retain valid external parents. It SHALL NOT offset objects or change scene camera/light settings. Missing external references SHALL fail the whole batch before mutation.

#### Scenario: Repeated paste
- **WHEN** the same clipboard is pasted repeatedly
- **THEN** each batch has independent identities, collision-free numbered names and the captured properties

#### Scenario: Parent changes
- **WHEN** a surviving external parent moves after copy
- **THEN** pasted objects retain captured local transforms under that parent and inherit its current world state

#### Scenario: Missing parent
- **WHEN** an external parent is deleted before paste
- **THEN** the entire paste is rejected rather than moved to scene root

### Requirement: Selection aware batch history
One successful paste SHALL create one history entry. Undo SHALL remove the complete batch and restore prior selection. Redo SHALL restore recorded values, names, hierarchy and selection without reading the clipboard. Restored handles SHALL remap all affected history references and clipboard external references. Save points SHALL behave like other authored transactions.

#### Scenario: Redo after clipboard replacement
- **WHEN** a paste is undone and the system clipboard is replaced with text
- **THEN** redo still restores that paste from history

### Requirement: Focus and interaction routing
Editor SHALL route exact nonrepeating Ctrl+C/Ctrl+V to scene clipboard operations only for focused scene panels outside text editing, active gestures and modal operations. Asset windows and unrelated panels SHALL NOT trigger scene paste. Text ownership in the input frame SHALL take precedence.

#### Scenario: Text editing
- **WHEN** Ctrl+V occurs in a name, search or numeric text editor
- **THEN** only text paste is performed and no scene nodes are created

#### Scenario: Copy immediately after placement
- **WHEN** a Place Object drag is successfully dropped into the viewport and the new object is selected
- **THEN** keyboard focus moves to the viewport so Ctrl+C followed by Ctrl+V copies that object without another click
- **AND** cancelled or failed placement does not transfer focus as a successful placement

### Requirement: Document and resource lifecycle
Snapshots SHALL be scoped to the current document and content environment and released on reset, detach or root retirement without clearing unrelated OS clipboard content. Same-document save SHALL retain snapshots. Resource preparation and stale document/revision checks SHALL use the shared domain rules; incompatible resources SHALL fail before scene mutation.

#### Scenario: Scene replacement
- **WHEN** the scene is closed, reopened or replaced or the content root changes
- **THEN** the old token cannot paste and old snapshot resource references are released

### Requirement: Shared typed automation
The system SHALL expose reflected copy, clipboard info and paste operations through the existing operation catalog. GUI and automation SHALL share clipboard, validation, transactions and persistence behavior. Missing clipboard providers SHALL return controlled unavailable errors, and results SHALL fit transport limits without embedding object snapshots.

#### Scenario: Agent and GUI share clipboard
- **WHEN** automation copies scene objects and GUI pastes, or GUI copies and automation pastes
- **THEN** both use the same snapshot and produce equivalent nodes, selection and one history entry

#### Scenario: Discovery without provider
- **WHEN** the scene clipboard provider is absent
- **THEN** operations remain describable with unavailable status and cannot report false success
