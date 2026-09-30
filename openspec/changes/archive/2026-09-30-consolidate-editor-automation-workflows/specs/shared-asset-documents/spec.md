## ADDED Requirements

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
