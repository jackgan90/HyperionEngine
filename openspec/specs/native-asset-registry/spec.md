# native-asset-registry Specification

## Purpose
Define rebuildable native asset discovery, portable ID resolution, duplicate identity diagnostics and metadata-only scanning without mandatory management files.
## Requirements
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

### Requirement: Format-owned metadata range discovery

Native asset discovery SHALL obtain native payload and archive metadata ranges from validated probes owned by the respective format modules. It SHALL NOT independently interpret their binary field offsets, directory entry sizes or version rules. Probes SHALL check supplied byte lengths before field access and reject unsupported or excessive declared ranges before the registry issues dependent reads. The registry SHALL continue to extract and validate the reflected asset header and preserve per-file errors, duplicate-ID rejection and existing indexing behavior.

#### Scenario: Current asset metadata
- **WHEN** discovery scans a valid HAST v1 asset containing a HYPA v2 archive
- **THEN** its header is equivalent to the encoded header and range selection comes from format-owned probes

#### Scenario: Truncated or short-returned prefixes
- **WHEN** a native/archive prefix or a returned directory/metadata range is shorter than required, including a provider returning fewer bytes than requested
- **THEN** discovery fails safely with a per-file error before any access outside the supplied span

#### Scenario: Excessive declared range
- **WHEN** a prefix declares a payload above the native file budget or an archive prefix/directory/metadata extent above the 32 MiB discovery budget
- **THEN** discovery rejects it before reading or allocating the excessive range

### Requirement: Lightweight discovery validation boundary

Discovery SHALL continue to require the current HAST v1 native container with HYPA v2 archive metadata and SHALL preserve range-only scanning on local and mounted storage. Native file limits SHALL include the native header, and metadata decoding SHALL retain archive node, depth and allocation budgets. Discovery SHALL validate the supplied prefix, directory and metadata structure without requiring bulk reads, a complete payload digest, object revision consistency or full dependency validation. Full loading SHALL preserve its existing actual-length, integrity and legacy-format validation independently.

#### Scenario: Large bulk data
- **WHEN** a current native asset contains a large bulk payload
- **THEN** local/mounted discovery reads only required prefix/directory/metadata ranges and does not read or hash the bulk to obtain its header

#### Scenario: Corrupt or truncated unseen bulk
- **WHEN** metadata and the declared directory remain valid but bulk bytes are corrupt or only the bulk portion has been truncated
- **THEN** discovery can still return the metadata while full loading rejects the asset

#### Scenario: Legacy full load
- **WHEN** a supported bare HYPA v1/v2 input is passed to full loading and discovery
- **THEN** full loading retains its legacy behavior and discovery rejects the input without broadening its accepted formats

#### Scenario: Malformed metadata structure
- **WHEN** metadata contains invalid directory boundaries, block indices, element alignment or exhausted node/depth/allocation budgets
- **THEN** discovery rejects the file through the format owner's metadata decoder without reading bulk
