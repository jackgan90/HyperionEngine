## ADDED Requirements

### Requirement: Configured target identity and coverage
Validation SHALL use executed CMake declarations to associate each owned target with its selected sources and direct link visibility. It MUST identify its configuration coverage and fail for stale metadata or unsupported graph-affecting declarations.

#### Scenario: Multiple targets and conditional links
- **WHEN** one module declares a library and a test linking a higher-level module under BUILD_TESTING
- **THEN** the test edge belongs only to the test target and the report identifies the configured branch selection

#### Scenario: Unsupported expression or stale graph
- **WHEN** an engine source/link expression cannot be interpreted or a graph input has changed since configuration
- **THEN** validation fails with a location or regeneration instruction instead of reporting a complete pass

### Requirement: Direct visibility and production boundaries
Production private includes SHALL have direct declared dependencies; public includes SHALL have direct PUBLIC or INTERFACE dependencies. Existing CPU domains including Environment MUST remain independent of Renderer, RHI and native backends, and Runtime MUST remain independent of concrete plugins. Actual production cycles MUST be diagnosed with edge locations and visibility.

#### Scenario: Private declaration of public dependency
- **WHEN** a public header includes another module whose edge is only PRIVATE
- **THEN** validation reports the source target, dependency, include location and visibility declaration

#### Scenario: Missing direct dependency masked by transitive export
- **WHEN** an implementation includes another module available only through a transitive chain
- **THEN** validation reports a missing direct declaration

#### Scenario: Real cycle and CPU violation
- **WHEN** actual production links form a cycle or a CPU module reaches a rendering module
- **THEN** validation reports the offending actual edges independently of test links

### Requirement: Private and vendor isolation
Both production and test source validation SHALL preserve module private boundaries and allow native SDK includes only in existing authorized private adapter/backend locations.

#### Scenario: Legal backend implementation
- **WHEN** a native backend private source includes its SDK
- **THEN** isolation validation accepts it without relaxing public or cross-module private-header rules

### Requirement: Public target consumers
Verified public dependency corrections SHALL be validated by compiled and linked consumers that directly link only the owning public target.

#### Scenario: Renderer and GUI diagnostics consumers
- **WHEN** consumers include NativeModel, RenderOutput or GUI diagnostics and link only the corresponding public target
- **THEN** configuration, compilation and linking succeed without full-application include propagation
