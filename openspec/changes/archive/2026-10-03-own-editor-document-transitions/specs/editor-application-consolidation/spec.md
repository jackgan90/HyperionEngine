## ADDED Requirements

### Requirement: Owned document transition rules

Editor SHALL own pending document-open, content-root and application-close decisions in its private document transition domain. Transition targets and progress phases SHALL be explicit; callers SHALL use semantic operations and read-only queries rather than directly mutate coordinated state. GUI and existing automation host operations SHALL retain their shared validation, history, persistence and lifecycle contracts. ContentRootService SHALL remain the final authority for root preparation, dirty/busy/generation checks and participant retirement.

#### Scenario: Cancel while admitted save is pending
- **WHEN** a root or close continuation is cancelled by a button, modal dismissal or the existing application-close operation while admitted saves are pending
- **THEN** those saves may finish against their original documents, but their completion does not switch roots or close the application
- **AND** pending decision presentation and continuation state do not reappear on subsequent polling

#### Scenario: Save failure and retry
- **WHEN** save admission fails, a scene save fails asynchronously, or settled saves leave documents dirty
- **THEN** the owner applies the existing target-specific failure/retry decision without losing the current document, history or dirty state
- **AND** application close status preserves the existing idle, saving, failed and closing string contract

#### Scenario: Close overlaps a root decision
- **WHEN** native application close is requested while a root decision remains pending
- **THEN** both intents remain represented for the existing UI and confirmed discard resolves close before root and scene-open replacement
- **AND** a cancelled or completed close cannot later commit an obsolete root continuation

#### Scenario: Prepared root commit and invalidation
- **WHEN** a root becomes ready through clean state, successful saves or explicit discard
- **THEN** the host revalidates and commits through ContentRootService, waits for accepted saves/edits and preserves its controlled failure and content invalidation behavior
- **AND** repeated polling cannot consume the same root or open action twice

#### Scenario: Existing GUI and automation access
- **WHEN** existing document, root and close workflows are discovered and used through GUI, CLI or MCP
- **THEN** operation IDs, schemas, revisions, errors, close reply completion and provider lifetime remain compatible
- **AND** the transition owner can be exercised without GUI rendering or transport-specific domain branches
