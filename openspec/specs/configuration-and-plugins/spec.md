# configuration-and-plugins Specification

## Purpose
Define stable reflected configuration and startup-selected static plugins with explicit activation authority, validated dependency planning, scoped rollback, and controlled optional-feature failure.
## Requirements
### Requirement: Reflected configuration round trip
The engine SHALL save and restore registered fields with stable type identity and schema version through its serialization interface.
#### Scenario: Save then load
- **WHEN** settings are saved and loaded into a new instance
- **THEN** persistent registered fields have the saved values without exposing JSON types

### Requirement: Validated configuration loading
Loading SHALL reject incompatible schema versions, invalid types and out-of-range fields before replacing application settings.
#### Scenario: Invalid input
- **WHEN** a configuration contains an invalid window width or a future version
- **THEN** loading fails with an actionable error and the settings remain unchanged

### Requirement: Dependency-aware static plugins
The registry SHALL detect duplicate IDs, missing dependencies, conflicts and dependency/order cycles before activating plugins. Required dependencies SHALL start before consumers; optional ordering SHALL only constrain selected plugins. Explicitly disabled plugins SHALL never be activated transitively. Tolerant activation SHALL report missing optional branches and preserve unrelated plugins; strict activation SHALL reject missing required plugins.

#### Scenario: Dependency startup
- **WHEN** a requested plugin depends on another registered plugin
- **THEN** the dependency starts first and shutdown occurs in reverse order

#### Scenario: Optional early plugin
- **WHEN** capture requests ordering before graphics and both are selected
- **THEN** capture starts first without graphics requiring capture when capture is absent

#### Scenario: Disabled dependency
- **WHEN** a requested feature needs an explicitly disabled plugin
- **THEN** the feature is unavailable with an actionable diagnostic and the disabled plugin remains inactive

#### Scenario: Strict preflight failure
- **WHEN** strict selection contains a missing plugin, disabled dependency or unavailable required service
- **THEN** activation fails before any selected plugin factory or Start function runs

### Requirement: Startup rollback
Plugin activation SHALL stop a partially started instance and undo its scoped registrations if it fails. Strict activation SHALL stop already-started plugins and report failure; tolerant activation SHALL skip dependent consumers and continue unrelated branches.

#### Scenario: Failing plugin
- **WHEN** a plugin throws during strict startup
- **THEN** prior plugins are stopped and the failure is reported

#### Scenario: Optional startup failure
- **WHEN** an optional plugin throws during tolerant startup
- **THEN** unrelated plugins remain active and no failed service or callback remains registered

### Requirement: Explicit activation authority
Persisted feature parameters SHALL NOT implicitly enable plugins. Explicit CLI feature selection SHALL update requested selection while honoring disabled-plugin configuration. Stable plugin IDs and existing serialized property keys SHALL remain supported.

#### Scenario: Stale scene source
- **WHEN** scene_source remains configured but scene-viewer is not selected
- **THEN** the scene-viewer plugin is not activated
