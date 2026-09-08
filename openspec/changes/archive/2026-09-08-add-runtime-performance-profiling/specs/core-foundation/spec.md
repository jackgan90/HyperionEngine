## MODIFIED Requirements

### Requirement: Observable application execution
The engine SHALL provide logging and named CPU timing scopes without exposing vendor types. Scope events SHALL be conditional on profiling being compiled in and activated; disabled scopes SHALL NOT require local clock measurements or aggregate scope-count updates.

#### Scenario: Instrumented startup
- **WHEN** the integration program emits a log message and executes a timing scope during an active capture
- **THEN** the log is written and the scope records its caller location and nonnegative measured duration

#### Scenario: Profiling disabled
- **WHEN** the same program executes with profiling compiled out
- **THEN** logging and tagged memory accounting remain available and scope instrumentation is eliminated
