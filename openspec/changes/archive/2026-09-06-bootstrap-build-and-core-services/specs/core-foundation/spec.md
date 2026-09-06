## ADDED Requirements

### Requirement: Reproducible local build
The project SHALL compile C++20 targets using checked dependency revisions and verified archive hashes.

#### Scenario: Clean build
- **WHEN** dependencies are bootstrapped and a preset is configured in an empty output directory
- **THEN** the core integration executable builds and runs without system-wide package installation

### Requirement: Third-party isolation
Engine modules SHALL access external libraries through engine-owned interfaces; vendor includes SHALL remain in private adapter implementations.

#### Scenario: Boundary validation
- **WHEN** the source-boundary check examines engine public headers and non-adapter sources
- **THEN** no third-party header is included

### Requirement: Tagged aligned memory
The allocation wrapper SHALL preserve requested alignment, reject invalid alignment, and account for live and peak requested bytes by category across threads.

#### Scenario: Allocate and free
- **WHEN** an aligned allocation is filled and freed
- **THEN** its address satisfies alignment and live allocation counters return to their previous values

### Requirement: Observable application execution
The engine SHALL provide logging and named CPU timing scopes without exposing vendor types.

#### Scenario: Instrumented startup
- **WHEN** the integration program emits a log message and executes a timing scope
- **THEN** the log is written and the scope has a nonnegative measured duration
