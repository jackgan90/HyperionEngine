## ADDED Requirements

### Requirement: Reproducible asset validation
The engine SHALL provide automated fixtures that run without network and verify supported format and asynchronous lifecycle behavior.

#### Scenario: Reproducible asset validation acceptance
- **WHEN** the CPU suite runs with one Worker
- **THEN** supported models load and cancellation/failure cases terminate without deadlock

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
