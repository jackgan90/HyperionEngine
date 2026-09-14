## MODIFIED Requirements

### Requirement: Portable asset identity and references
Asset references SHALL carry identity, expected type and portable location information with optional pinned revision. Multiple catalogs SHALL resolve IDs and reject conflicting duplicates; reference resolution SHALL validate target identity/type/revision. Mounted assets SHALL use canonical package identities for caches, dependency graphs and ordered writes. Legacy relative references SHALL remain readable. Saving mounted scenes SHALL preserve package references across roots.

#### Scenario: Relocate a content graph
- **WHEN** a native scene and dependencies are moved and mounts are updated
- **THEN** references resolve independently of source directory or process working directory

#### Scenario: Cross-root persistence
- **WHEN** a Game scene referring to Engine resources is saved and reloaded
- **THEN** its references retain virtual locations and checked identities without embedding local disk paths
