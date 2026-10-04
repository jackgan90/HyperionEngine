## ADDED Requirements

### Requirement: Known asset identity consumers use owning descriptors
Consumers in AssetImport, AssetTool, Editor and Scene with existing direct dependencies SHALL derive known texture, material, model and scene identities from their owning `RecordType<T>()` descriptors. Existing serialized IDs, reference metadata, discovery, validation and routing outcomes MUST remain unchanged.

#### Scenario: Existing native assets are consumed
- **WHEN** existing textures are published or migrated, scenes are discovered/opened, or model/material references are validated and constructed
- **THEN** the consumers use descriptor identities and preserve the previous accepted types, reference strings and operation outcomes

#### Scenario: Legacy output layout is retained
- **WHEN** legacy migration selects an asset product path, including a type without a known directory mapping
- **THEN** the existing directory mapping and unknown-type `Assets` fallback remain unchanged

### Requirement: Image importer identity has one owner
AssetImport SHALL define the stable image importer ID once for registration and consumption. Direct import and prepared import SHALL retain image naming, texture root type and importer provenance behavior.

#### Scenario: Image import with a requested name
- **WHEN** a PNG is imported directly or prepared and then published with a caller-provided name
- **THEN** the texture has the requested name and its persisted importer provenance remains `hyperion.image` version 1 with the existing texture type ID

### Requirement: Matrix inspection uses C++ type identity
Gui SHALL select its matrix control using the descriptor C++ type identity for `FMat4`, without adding a Scene dependency. Existing matrix inspection and writeback SHALL remain available independently of the descriptor ID string.

#### Scenario: Matrix descriptor ID differs
- **WHEN** an inspected `FMat4` uses an otherwise equivalent descriptor with a different ID
- **THEN** Gui renders the matrix control and numeric input updates the matrix draft correctly

#### Scenario: Existing reflected type schemas remain stable
- **WHEN** existing texture, material, model and matrix types are described or serialized
- **THEN** their type IDs, members, schema and existing external metadata remain unchanged
