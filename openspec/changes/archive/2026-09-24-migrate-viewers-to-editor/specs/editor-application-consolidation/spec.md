## ADDED Requirements

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
