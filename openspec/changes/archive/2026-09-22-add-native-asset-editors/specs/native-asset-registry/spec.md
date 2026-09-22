## MODIFIED Requirements

### Requirement: Reconstructible native registry
Assets SHALL discover ID, type, current digest and path from native files into a non-persistent in-memory index. Persistent Catalog assets, their type registration and their creation command SHALL be removed. Discovery SHALL omit Git, caches and publication state, reject conflicting duplicate IDs, and avoid reading bulk payloads on local/mounted storage. Loading SHALL still validate full integrity.

#### Scenario: Clone and move
- **WHEN** a content directory is cloned elsewhere and an asset is moved within it before registry rebuild
- **THEN** its existing ID resolves to the new package path without editing referring assets or preserving a machine-specific registry

#### Scenario: Duplicate identity
- **WHEN** two files claim the same asset ID
- **THEN** discovery reports their conflicting paths without silently selecting one

#### Scenario: Missing management files
- **WHEN** a directory contains valid native assets and no Catalog or library file
- **THEN** application startup and content-root selection discover its assets normally

#### Scenario: Retired catalog
- **WHEN** the old Catalog type is requested
- **THEN** it is not a registered native asset type and no specialized editor or persistent writer is available
