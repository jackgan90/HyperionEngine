# external-content-repository Specification

## Purpose
Define engine and sample content ownership, native distribution through Git LFS, recoverable source recipes and migration acceptance.
## Requirements
### Requirement: Content ownership and distribution
The engine SHALL track engine-essential Content including text shaders and shared BRDF data. HyperionAssets SHALL contain visible current native assets and supported text shaders, track hassets with Git LFS and retain license metadata. Historical native generations and mandatory management hassets SHALL be removed. Runtime use SHALL not require original inputs or source provenance.

#### Scenario: Native-only distribution
- **WHEN** external content is cloned to a different local path with LFS objects and mounted as Game
- **THEN** all migrated roots and dependencies load without original source assets or persisted catalog/library files

### Requirement: Recoverable content recipes
Sample source recipes SHALL be optional development tooling. When retained, their source identities and publication configuration SHALL be portable and rebuilding SHALL use the same native importer. Arbitrary native-only content roots SHALL not require a manifest.

#### Scenario: Rebuild on another machine
- **WHEN** unchanged sample inputs are available under a different physical source cache
- **THEN** reimport of mapped existing targets retains their native identities without persisting that cache path

### Requirement: Migration acceptance
Migration SHALL preserve every currently usable public asset and dependency, authored scene edits and valid cross-mount sharing. It SHALL not restore absent historical library entries. Acceptance SHALL verify complete dependency closure, visible single-current identities, source independence, relocation, scene save/reload, shaders and rendering. Migration SHALL not commit either repository without explicit user authorization.

#### Scenario: New structure acceptance
- **WHEN** the former .assets and management files are absent
- **THEN** all migrated content operates from its visible native files and reconstructed registry
