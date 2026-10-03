## ADDED Requirements

### Requirement: Shared sky conversion settings validation

AssetImport preflight SHALL consume Environment's authoritative sky bake-settings validation while retaining its HDR/EXR and asset-type restrictions. Reflected setting IDs, versions, defaults and descriptions and existing invalid_argument messages SHALL remain unchanged. GUI and automation SHALL continue to use the same import domain.

#### Scenario: Preflight and bake agree
- **WHEN** a compatible HDR/EXR sky import is checked with any supported or invalid bake setting tuple
- **THEN** its numeric admission agrees with Environment without duplicating the rules

#### Scenario: Incompatible import kind
- **WHEN** sky settings accompany an unsupported source extension or another asset type
- **THEN** import preflight rejects the request even when its numeric settings are valid

#### Scenario: Existing clients discover defaults
- **WHEN** a client reads the sky settings reflection or submits existing fields
- **THEN** the field shape, descriptions, defaults and existing completion/publication behavior remain compatible
