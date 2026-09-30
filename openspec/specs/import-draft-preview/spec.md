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

### Requirement: Semantic import choices and typed preview metadata
Import GUI choices and applicable settings SHALL resolve stable capability type IDs rather than numeric positions. Texture draft inspection SHALL expose reflected typed dimensions and pixel byte size shared by GUI and automation. GUI SHALL not parse human-readable Details to recover those fields. Preview pagination SHALL use an explicit consistent limit for requests and navigation.

#### Scenario: Capability ordering changes
- **WHEN** supported import capabilities are presented in a different order
- **THEN** selecting a type still produces its correct stable type ID, allowed source filters and applicable settings without adding unsupported formats

#### Scenario: Inspect a texture through either surface
- **WHEN** GUI or automation inspects a prepared texture draft
- **THEN** both observe matching typed dimensions and byte size, existing Details remains compatible, and discovery describes the additive fields

#### Scenario: Multiple pages of preview nodes
- **WHEN** the user advances and returns through preview pages
- **THEN** query limits and navigation offsets agree with no skipped or repeated page caused by inconsistent limits
