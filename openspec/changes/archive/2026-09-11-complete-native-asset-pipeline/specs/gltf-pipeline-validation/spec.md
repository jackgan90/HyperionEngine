## MODIFIED Requirements

### Requirement: Reproducible asset validation
The engine SHALL provide offline fixtures verifying source import, native persistence, schema compatibility, dependency/cache behavior and asynchronous lifecycle. Importer tests SHALL retain external format coverage and runtime tests SHALL consume generated native assets.

#### Scenario: Reproducible asset validation acceptance
- **WHEN** the CPU suite runs with one Worker
- **THEN** imports, native loads and cancellation/failure/cycle cases terminate without deadlock

## ADDED Requirements

### Requirement: Native runtime independence
Runtime acceptance SHALL deny asset access to original glTF/GLB, bin, image and scene JSON source files while rendering imported model and scene assets. Shaders, application configuration and GUI resources SHALL remain separately available.

#### Scenario: Native-only content
- **WHEN** only native assets and their portable catalog/dependencies are available to asset IO
- **THEN** model and scene rendering succeeds with expected instance counts, material/depth/shadow evidence and zero D3D12 validation errors

### Requirement: Asset pipeline measurement
Validation SHALL report source conversion versus native loading time, bytes/read count and peak memory with build/configuration context and readiness checks.

#### Scenario: Repeat native load
- **WHEN** repeated cold-service source conversion and native loads of the same fixture are measured
- **THEN** results identify conversion work removed and remaining preparation costs without claiming unmeasured GPU improvements
