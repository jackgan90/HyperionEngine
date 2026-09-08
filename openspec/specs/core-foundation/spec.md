# core-foundation Specification

## Purpose
TBD - created by archiving change bootstrap-build-and-core-services. Update Purpose after archive.
## Requirements
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
The engine SHALL provide logging and named CPU timing scopes without exposing vendor types. Scope events SHALL be conditional on profiling being compiled in and activated; disabled scopes SHALL NOT require local clock measurements or aggregate scope-count updates.

#### Scenario: Instrumented startup
- **WHEN** the integration program emits a log message and executes a timing scope during an active capture
- **THEN** the log is written and the scope records its caller location and nonnegative measured duration

#### Scenario: Profiling disabled
- **WHEN** the same program executes with profiling compiled out
- **THEN** logging and tagged memory accounting remain available and scope instrumentation is eliminated
