# editor-application-consolidation Specification

## Purpose
Define Editor as the maintained graphics application and preserve reusable functionality and validation when retiring experimental hosts.
## Requirements
### Requirement: Single maintained graphics application
The build SHALL provide Editor as the normal graphics application and SHALL remove obsolete standalone scene/model inspection applications, plugins and build selection. Native asset loading, shared runtime algorithms and independent AssetTool/Automation applications SHALL remain available.

#### Scenario: Fresh build and IDE startup
- **WHEN** the project is configured from clean build metadata
- **THEN** Editor is the default IDE startup application and no removed plugin target is required to compile or link it

### Requirement: Preserve validation coverage
Useful rendering, resource, input, capture, automation, shutdown and benchmark scenarios SHALL execute through Editor or focused test-only runtime consumers. Removing optional experiment plugins SHALL NOT silently switch the project to a reduced unrelated test suite.

#### Scenario: Optional feature disabled
- **WHEN** an optional feature is disabled at build or startup
- **THEN** unrelated tests remain registered and applicable absence, startup failure and shutdown checks run

### Requirement: Current documentation and tools
Active documentation, specifications and executable tool commands SHALL describe supported Editor and runtime workflows. Obsolete application-specific documentation and launch commands SHALL be removed without relabelling historical measurements as new results.

#### Scenario: Follow a documented rendering command
- **WHEN** a developer follows the supported build, capture or profiling workflow
- **THEN** it uses shipped targets and does not require the removed application

### Requirement: Integrated Editor interaction surfaces
The Edit menu SHALL omit Scene settings, Duplicate primary object and Delete primary, keep children. Existing object-oriented scene role controls and Details hierarchy editing SHALL remain available. Shared duplication and keep-children deletion transactions and Automation operations SHALL remain available; their GUI entry points SHALL be deferred until an Outliner or viewport interaction is designed.

#### Scenario: Open the Edit menu
- **WHEN** a user opens Edit with a loaded scene and selected object
- **THEN** the three removed entries are absent and existing Undo/Redo remain available

#### Scenario: Use the retained editing capabilities
- **WHEN** a user edits through existing scene role or hierarchy controls, or an agent duplicates or removes the primary object while retaining children
- **THEN** the operation continues through the existing shared scene document validation and history

### Requirement: Cohesive Editor ownership and isolated acceptance driver
Editor SHALL keep feature lifecycle and typed service registration in its plugin while assigning viewport behavior and pending document transitions to focused private owners. Acceptance-only state and routing SHALL live behind a distinct driver with semantic phases and explicit input-window selection. Production input routing SHALL not interpret numeric acceptance-step ranges. A BUILD_TESTING-disabled Editor SHALL compile and link without acceptance implementation sources.

#### Scenario: Test-disabled build
- **WHEN** Editor is configured with BUILD_TESTING disabled
- **THEN** it builds and links without acceptance implementation and reports requested unavailable acceptance behavior as a controlled diagnostic

#### Scenario: Test-enabled lifecycle
- **WHEN** existing Editor acceptance scenarios run with tests enabled, including optional provider absence and shutdown
- **THEN** their input targets, useful coverage, provider lifetime and work-drain behavior remain intact

#### Scenario: Pending transition and rendering
- **WHEN** a dirty document transition is confirmed or cancelled while viewport work exists
- **THEN** existing busy/discard decisions, document identity, frame snapshot ownership and GPU retirement behavior remain unchanged
