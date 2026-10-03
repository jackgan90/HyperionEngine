## ADDED Requirements

### Requirement: Typed import lifecycle with compatible status projection

Import tasks and import drafts SHALL have distinct domain-owned typed states. Runtime and GUI decisions SHALL use those states. Existing string status tokens, reflected schema shape, field IDs, generation and busy/dirty behavior SHALL remain compatible; unknown boundary tokens SHALL be rejected.

#### Scenario: Task completion and failure
- **WHEN** an accepted import finishes or fails
- **THEN** its typed state maps to the existing completed or failed token and retains the existing result/error and non-cancellable lifetime

#### Scenario: Draft preparation and publication
- **WHEN** a draft is prepared, published, fails preparation, fails publication or is discarded
- **THEN** the existing preparing/ready/publishing/failed/discarded statuses remain observable
- **AND** publication failure restores Ready with its error and unchanged save point so editing/retry remain possible

#### Scenario: Boundary compatibility
- **WHEN** a known status is encoded, described and decoded
- **THEN** it remains a string with the existing token and schema; empty uninitialized preview snapshots remain empty
- **AND** invalid status tokens do not become valid lifecycle states
