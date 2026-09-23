## ADDED Requirements

### Requirement: One typed operation contract
The engine SHALL provide a protocol-independent operation catalog whose typed registration supplies request/result schemas, implementation, identity, version, availability, side-effect and usage documentation. Catalog admission and invocation SHALL respect its owning Main thread and plugin scope.

#### Scenario: A new operation is registered
- **WHEN** a provider registers a typed operation and seals the catalog
- **THEN** search, describe, CLI and MCP can use it without transport-specific registration
- **AND** duplicate registrations and registration after sealing are rejected

#### Scenario: A provider is unavailable
- **WHEN** the asset provider is disabled or fails to start
- **THEN** discovery remains usable and dependent operations return a structured unavailable result without dereferencing a missing service

#### Scenario: Adapter startup fails after registration
- **WHEN** an adapter fails after adding some operations to the catalog
- **THEN** scoped plugin cleanup withdraws its callable closures before destroying provider state
- **AND** unrelated discovery and session services remain usable

### Requirement: Reflected strict wire values
The engine SHALL derive natural-JSON schemas and codecs from reflected values without changing native persistence. It SHALL reject unknown/duplicate fields, missing required fields, invalid numeric/enum values and incorrect fixed-array lengths before invoking a handler, and preserve wide integers losslessly.

#### Scenario: Invalid input never reaches a handler
- **WHEN** an operation receives malformed or schema-invalid parameters
- **THEN** a structured error identifies the problem and the handler has no effects

#### Scenario: An input type gains a field
- **WHEN** its reflected contract gains a documented field
- **THEN** the same registration exposes the field in describe and accepts it through both transports

### Requirement: Progressive capability discovery
The endpoint SHALL provide bounded deterministic search summaries and on-demand operation/type descriptions including input/output schemas, usage, side effects and completion semantics. Initial MCP declarations SHALL remain a small fixed bootstrap set.

#### Scenario: Discover without source reading
- **WHEN** an agent searches for asset renaming and describes the matching operation
- **THEN** it receives enough information to construct the request, interpret its result and understand that saving is separate

### Requirement: Owned asynchronous jobs
Admitted deferred operations SHALL have session-owned job IDs, observable terminal results/errors and explicit cancellation support. Jobs and result retention SHALL be bounded; shutdown SHALL reject new calls and drain admitted work before dependencies are destroyed.

#### Scenario: Save completion and failure
- **WHEN** an asynchronous save is admitted
- **THEN** the initial result identifies a job and polling reports actual persistence completion or failure
- **AND** unsupported cancellation does not claim the write was rolled back

### Requirement: Shared CLI and MCP execution
The application SHALL support one-shot CLI queries, a persistent JSON-lines session and MCP stdio using the same endpoint. MCP SHALL negotiate the supported protocol revision and expose only implemented capabilities. stdout SHALL contain only framed results and notifications SHALL not receive responses.

#### Scenario: Equivalent calls
- **WHEN** CLI and MCP invoke the same operation with equivalent session state and inputs
- **THEN** they produce equivalent domain results and effects

#### Scenario: Stream termination
- **WHEN** input reaches EOF with admitted asset work
- **THEN** the application completes orderly plugin shutdown without leaking tasks or accessing destroyed state

#### Scenario: Bounded response framing
- **WHEN** a valid bounded domain result requires additional MCP text/structured duplication, escaping, job metadata or a large request ID
- **THEN** a separate bounded response budget accommodates the complete envelope without changing the request or domain payload limits
- **AND** JSONL preserves correlation and accepts subsequent requests after a recoverable response failure

#### Scenario: Result cannot be encoded after execution
- **WHEN** an invoked handler produces a result exceeding the domain result budget
- **THEN** the call or job reports `result_unavailable` and warns that effects may already have occurred
- **AND** it does not label the accepted request as invalid arguments or claim rollback

### Requirement: Documented extension and coverage contract
Repository development documentation SHALL require shared domain logic, reflected operation registration and an explicit coverage status for new user-facing capabilities. Existing unadapted areas SHALL be listed without implying complete API coverage.

#### Scenario: A new feature is developed
- **WHEN** a developer follows the extension guide
- **THEN** the guide demonstrates registration, schema/usage queries and validation without editing the CLI or MCP dispatch tables
