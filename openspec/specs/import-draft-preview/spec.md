# import-draft-preview Specification

## Purpose
Prepare unpublished asset drafts for type-specific property inspection, restricted editing and history before confirmed publication through equivalent GUI and automation operations.
## Requirements
### Requirement: Unpublished preparation
The system SHALL asynchronously prepare a converted root and its dependencies without publishing files, and expose type-specific metadata without bulk geometry or pixels.

#### Scenario: Prepare and discard
- **WHEN** a user selects a valid source and later discards its preview
- **THEN** no destination or dependency files have been published

### Requirement: Restricted draft editing and history
The system SHALL allow root names, model node names/local transforms, primitive names/existing material indices and allowed material numeric values; it SHALL preserve identity/topology and provide undo, redo and reset. Scene contents and generated dependencies SHALL remain read-only.

#### Scenario: Edit a model
- **WHEN** a node transform or primitive material index is edited
- **THEN** validation preserves topology and stable IDs, and undo restores the prior value

#### Scenario: Unsupported modification
- **WHEN** an edit targets a missing node or a read-only material parameter
- **THEN** the draft and its version remain unchanged and an error is returned

### Requirement: Confirmed publication
The system SHALL publish the confirmed draft through existing transactional publication and record property overrides in import provenance. It SHALL reject changed sources before publication, including forced imports.

#### Scenario: Repeat identical edits
- **WHEN** identical source, settings and overrides are submitted again
- **THEN** the existing freshness mechanism reports up-to-date and writes zero assets

#### Scenario: Source changed during preview
- **WHEN** a captured source or dependency changes before submission completes
- **THEN** publication fails without writing the stale draft

### Requirement: GUI and automation equivalence
The system SHALL expose shared preparation, inspection, edit, history, submission and discard contracts with generation checks. The GUI SHALL open a separate property window, mark changed input/settings stale and require explicit discard before replacing modified drafts.

#### Scenario: Agent inspects GUI draft
- **WHEN** an agent queries a draft created by the GUI
- **THEN** it observes the same values and can perform equivalent validated edits

#### Scenario: Root lifecycle
- **WHEN** a root change is requested with preparing work or dirty drafts
- **THEN** the shared root service enforces busy/dirty policy and invalidates discarded draft IDs after a successful change
