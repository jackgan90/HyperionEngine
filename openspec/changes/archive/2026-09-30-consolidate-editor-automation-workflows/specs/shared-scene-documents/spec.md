## ADDED Requirements

### Requirement: Validation independent of scene enumeration
Single-node authoring and component validation SHALL check document identity, revision and live handles directly without enumerating all scene nodes. Batch preparation SHALL validate common document state once and validate every requested target before mutation. Existing error contracts and atomicity SHALL remain unchanged.

#### Scenario: Large scene single-node edit
- **WHEN** a valid single-node property or component edit is prepared against a large scene
- **THEN** document validation performs no full-scene node enumeration and preserves the existing edit result

#### Scenario: Invalid member in a batch
- **WHEN** a batch includes a stale, foreign or invalid target
- **THEN** the entire operation fails without partial mutation and without per-member full-scene enumeration

### Requirement: Shared ordered selection roots
Operations requiring top-level selected roots SHALL share an ordered hierarchy filter. It SHALL stop walking a node's ancestry once a selected ancestor covers it, preserve selected-root order and retain each operation's handle validation and transaction semantics.

#### Scenario: Fully selected deep hierarchy
- **WHEN** every node of a deep parent-child chain is selected for a roots-based operation
- **THEN** only the top root is returned and filtering does not traverse every ancestor for every descendant

#### Scenario: Mixed roots and invalid input
- **WHEN** independent branches and descendants are selected, or a caller supplies invalid handles
- **THEN** valid roots retain input order and invalid inputs follow that caller's existing atomic failure contract
