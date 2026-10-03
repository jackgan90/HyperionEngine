## ADDED Requirements

### Requirement: Isolated acceptance discovery lifecycle
Acceptance launches SHALL use a unique per-run local discovery environment shared by their application and frontend children. They SHALL NOT publish to or purge the interactive user's discovery directory. Local registration maintenance SHALL remain bounded at publication boundaries, preserve normal plugin withdrawal, and SHALL NOT add per-frame cleanup or logging.

#### Scenario: Repeated terminated acceptance host
- **WHEN** an acceptance host is forcibly terminated and another acceptance run starts
- **THEN** leftover registrations cannot pollute the interactive user's discovery or hide the new run's exact known instance

#### Scenario: Shared child discovery environment
- **WHEN** an acceptance script launches Editor, CLI and MCP children
- **THEN** all children inherit the same isolated run environment unless a test explicitly overrides it for failure coverage
