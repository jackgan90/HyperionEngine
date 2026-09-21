## MODIFIED Requirements

### Requirement: Portable asset identity and references
Authoring references SHALL carry stable identity, expected type and package or content-relative location hints and SHALL follow current asset contents. Content revision SHALL remain an integrity/cache value; authoring save/import SHALL not pin references. Registry resolution SHALL reject conflicting identities and validate the resolved type and ID. Mounted caches, graphs and writes SHALL use canonical package paths. Explicit low-level revision requests SHALL remain validated.

#### Scenario: Relocate a content graph
- **WHEN** a native scene and dependencies are moved and mounts are updated
- **THEN** references resolve independently of source directory or process working directory

#### Scenario: Shared dependency edit
- **WHEN** two saved scenes reference one model or material and that dependency changes at its stable identity
- **THEN** reloading both scenes obtains the new content without rewriting either scene

#### Scenario: Cross-root persistence
- **WHEN** a Game scene referring to Engine resources is saved and reloaded
- **THEN** its references retain virtual locations and checked identities without embedding local disk paths or fixed revisions
