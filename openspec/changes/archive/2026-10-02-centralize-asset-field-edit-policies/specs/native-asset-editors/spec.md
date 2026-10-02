## ADDED Requirements

### Requirement: Canonical asset field editing policy

AssetEditing SHALL associate editable field access, validation/normalization, reference preparation and effects with the canonical reflected member identity. GUI, standalone and attached automation SHALL use that same domain policy while preserving external operation IDs, schemas and persisted fields. Reflection SHALL NOT own asset editing or preview policy. Generic write availability SHALL NOT be inferred merely from reflected readability, and dedicated primitive and texture encoding operations SHALL retain their restrictions.

#### Scenario: Same member through different callers
- **WHEN** GUI and either automation provider edit the same supported field from equivalent document states
- **THEN** normalization, validation, reference requirements, committed values and permitted operation route agree

#### Scenario: Read-only or dedicated field
- **WHEN** a readable field is read-only or requires a dedicated primitive/encoding operation
- **THEN** generic Set does not expose a new write operation or bypass the existing identity, topology, geometry or encoding restrictions

### Requirement: Asset domain owns preview change effects

Preview invalidation SHALL derive from normalized before/after values in the asset domain and SHALL NOT be supplied or overridden by adapter callers. Asset names, model-node names and primitive names alone SHALL preserve preview generation. Model-node Local changes, primitive material assignment changes, effective material/slot value changes and committed encoding/payload changes SHALL invalidate as appropriate. History SHALL retain the computed effects so Undo, Redo, Cancel and coalesced interactions apply equivalent invalidation while preserving existing generation and persistence behavior.

#### Scenario: Node rename through automation
- **WHEN** model.nodes.set changes only node names while retaining identities, topology and Local values
- **THEN** the persisted draft and history update with the same preview-generation behavior as the corresponding GUI edit, without rebuilding the preview

#### Scenario: Mixed name and transform change
- **WHEN** one node edit changes its name and Local transform
- **THEN** the domain records the existing transaction and invalidates the preview for the transform effect, including Undo/Redo

#### Scenario: Primitive metadata and material
- **WHEN** an existing primitive is renamed or assigned another valid existing material slot
- **THEN** rename alone preserves preview generation while material change invalidates, independently of caller

### Requirement: Prepared asset policies preserve bounded asynchronous completion

Prepared policy values and reference requirements SHALL be owned. Completion SHALL retain current document identity/generation checks and required reference type/dimension/graph validation before applying an edit. Closed/replaced/stale documents and failed requirements SHALL leave authoritative values and history unchanged. Drain and destruction SHALL join/release accepted work without committing. Texture root edits SHALL preserve exact encoding/mip history and shared unchanged bulk storage.

#### Scenario: Prepared reference becomes stale
- **WHEN** reference preparation completes after document generation or identity changes
- **THEN** the edit is rejected without changing the new document or history and admission is released

#### Scenario: Drain pending encoding or references
- **WHEN** a pending edit is drained during closure
- **THEN** work is joined and ownership released without applying the candidate or dirtying another document
