## ADDED Requirements
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
The registry SHALL detect duplicate IDs, missing dependencies and dependency cycles before activating plugins.
#### Scenario: Dependency startup
- **WHEN** a requested plugin depends on another registered plugin
- **THEN** the dependency starts first and shutdown occurs in reverse order
### Requirement: Startup rollback
Plugin activation SHALL stop already-started plugins if a subsequent plugin fails to start.
#### Scenario: Failing plugin
- **WHEN** a plugin throws during startup
- **THEN** prior plugins are stopped and the failure is reported
