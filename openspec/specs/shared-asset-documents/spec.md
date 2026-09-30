# shared-asset-documents Specification

## Purpose
Define the shared CPU asset-document authority used by Editor and automation for revision-aware editing, history, texture encoding and conflict-aware persistence.
## Requirements
### Requirement: Shared CPU document authority
The Editor and automation SHALL use the same Runtime asset-document implementation for draft state, history, save state and texture encoding edits. It SHALL not depend on GUI, Renderer, RHI or concrete plugins.

#### Scenario: Human and agent rename an asset
- **WHEN** equivalent name edits are made through GUI and automation domain calls
- **THEN** draft values, dirty state, undo/redo, preview generation and persisted results follow the same behavior

#### Scenario: Texture encoding changes
- **WHEN** either caller requests a supported encoding change
- **THEN** the shared CPU function rebuilds derived mips while preserving top-level pixels and shared history payloads

### Requirement: Revision-aware automated editing
Automated mutations SHALL require the current document generation and use session-scoped document identities. Stale, closed, dirty-close and busy document requests SHALL produce explicit errors without unintended edits.

#### Scenario: Stale mutation
- **WHEN** a mutation supplies a generation preceding an intervening edit or undo
- **THEN** the document remains unchanged and the caller receives the current generation

#### Scenario: Close a dirty document
- **WHEN** a caller closes a dirty document without an explicit discard policy
- **THEN** the operation fails and retains the document and history

### Requirement: Preserve save and history semantics
Shared documents SHALL preserve existing history grouping and captured-save-point behavior, including edits during an admitted save and conflicts with changed disk content.

#### Scenario: Edit while saving
- **WHEN** a save captures a document and another edit follows
- **THEN** successful save completion marks only the captured state as saved and the newer edit remains dirty

#### Scenario: Disk conflict
- **WHEN** another writer changed the asset identity or digest after opening
- **THEN** save fails, preserves dirty state and does not silently replace that writer's content

### Requirement: Shared asynchronous asset edit orchestration
Editor and automation SHALL use one CPU domain workflow for asynchronous texture encoding and asset reference edits. It SHALL own admission, preparation snapshots, busy lifetime, completion validation and transactional commit. Document identity and generation SHALL be revalidated before completion commits. Worker preparation SHALL not mutate live documents, and Main SHALL publish successful edits through shared history without implicit save.

#### Scenario: Equivalent edits through both callers
- **WHEN** GUI and automation request equivalent supported encoding or reference edits against equivalent documents
- **THEN** validation, resulting draft, generation, dirty state, undo/redo and failure behavior are equivalent

#### Scenario: Stale or failed preparation
- **WHEN** preparation fails or its captured document/generation is no longer current
- **THEN** completion returns a controlled error, releases its busy ownership and leaves draft, history and persisted files unchanged

#### Scenario: Close and shutdown with pending work
- **WHEN** an asset workspace closes, an automation session ends or its plugin quiesces with admitted asynchronous editing
- **THEN** existing busy/close policies are enforced and work is drained before captured state is destroyed, with no late commit into a replacement document
