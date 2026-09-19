## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Explicit activation authority
Persisted feature parameters SHALL NOT implicitly enable plugins. Explicit CLI feature selection SHALL update requested selection while honoring disabled-plugin configuration. Stable plugin IDs and existing serialized property keys SHALL remain supported.

#### Scenario: Stale scene source
- **WHEN** scene_source remains configured but scene-viewer is not selected
- **THEN** the scene-viewer plugin is not activated
