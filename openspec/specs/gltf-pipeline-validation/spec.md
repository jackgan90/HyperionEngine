# gltf-pipeline-validation Specification

## Purpose
Verify the supported asset pipeline using offline fixtures, lifecycle regressions, GPU readback and documented usage and limits.
## Requirements
### Requirement: Reproducible asset validation
The engine SHALL provide offline fixtures verifying source import, native persistence, schema compatibility, dependency/cache behavior and asynchronous lifecycle. Importer tests SHALL retain external format coverage and runtime tests SHALL consume generated native assets.

#### Scenario: Reproducible asset validation acceptance
- **WHEN** the CPU suite runs with one Worker
- **THEN** imports, native loads and cancellation/failure/cycle cases terminate without deadlock

### Requirement: Visible rendering evidence
The engine SHALL verify model output by GPU readback and retained screenshots in addition to CPU parsing tests.

#### Scenario: Visible rendering evidence acceptance
- **WHEN** the model acceptance test renders a known asymmetric textured fixture
- **THEN** pixel coverage, depth and material evidence pass and a screenshot is available

### Requirement: Documented support boundary
The engine SHALL document supported features, exclusions, usage and concrete verification commands.

#### Scenario: Documented support boundary acceptance
- **WHEN** a developer follows the model-viewer instructions
- **THEN** they can load a supported asset and identify why an unsupported asset is rejected

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

### Requirement: Shared asset pipeline evidence
Validation SHALL cover reflected material/texture round trips, corrupt references, shared library reimport, legacy split upgrade, custom shader execution, isolated edits and saved scene reload. GPU tests SHALL consume native-only content and report relevant resource identities/counters plus D3D12 validation status in Debug and Release.

#### Scenario: End-to-end independent assets
- **WHEN** two distinct models sharing assets are rendered, edited, saved and reloaded with source asset IO denied
- **THEN** expected pixels, sharing counts, scene state and zero D3D12 validation errors are verified

#### Scenario: Publication and lifetime failure cases
- **WHEN** shared reimport fails, a pinned revision mismatches or old GPU work remains in flight during an edit
- **THEN** prior roots remain valid, mismatches fail explicitly and resource retirement remains fence-safe
