## ADDED Requirements

### Requirement: Shared typed automation response boundary

Runtime/Automation SHALL own construction and typed structural reading of direct outcomes and job responses. Existing completed/failed/running/cancelled JSON, error fields, polling metadata, unwrapped bootstrap objects, CLI output/exit behavior, MCP outer isError flags and payload/frame budgets SHALL remain compatible. All response consumers SHALL use the shared interpretation rather than independently parsing protocol status text.

#### Scenario: Original valid response snapshots
- **WHEN** synchronous or asynchronous work completes, fails or is cancelled
- **THEN** emitted JSON matches the previous envelope after normalizing only ephemeral job identity
- **AND** session retention, cancellation capability and provider draining are unchanged

#### Scenario: Bootstrap and operation payload distinction
- **WHEN** a catalog query returns an unwrapped object or an operation returns a payload containing its own status field
- **THEN** the query stays unwrapped and the operation's payload is treated as opaque data

#### Scenario: Frontend terminal behavior
- **WHEN** CLI or MCP reads a running, completed, failed or cancelled job
- **THEN** it preserves existing waiting, terminal outcome output, failure exit and outer MCP isError behavior

#### Scenario: Malformed endpoint result
- **WHEN** a reply has unknown status, missing or mistyped fields, conflicting envelope fields or inconsistent job/outcome state
- **THEN** consumers report a controlled response/protocol error without treating it as success or replaying effects
