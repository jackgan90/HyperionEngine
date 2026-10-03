# automation-transport Specification

## Purpose
Define portable, bounded byte transports, target discovery, identity verification and admission policies for automation connections without coupling domain operations to a platform or device topology.
## Requirements
### Requirement: Replaceable asynchronous byte transports
The engine SHALL expose native-independent connection, listener and provider interfaces for reliable ordered full-duplex bytes. Implementations SHALL support bounded asynchronous progress, partial reads/writes, EOF/errors and close without blocking Main or borrowing ephemeral request buffers. A provider registry SHALL select explicitly registered schemes without domain branches.

#### Scenario: Substitute transport
- **WHEN** equivalent byte sequences arrive through Windows pipes or the memory test provider with different fragment boundaries
- **THEN** the shared protocol produces equivalent requests and results without provider-specific dispatch

#### Scenario: Close pending IO
- **WHEN** a connection or listener closes while native IO is pending
- **THEN** admission stops and pending state is cancelled or completed before its storage is destroyed

### Requirement: Independent target discovery and identity
Discovery SHALL return advisory target descriptors with boot-specific instance identity and transport addresses. Direct address connection SHALL not require local discovery or a local PID. The handshake SHALL verify expected identity and compatible communication versions before accepting calls. Lookup of an exact known instance SHALL be independent of enumeration result and scan limits. Bounded enumeration SHALL report typed completion or limit reason, derived truncation and examined/stale-skipped/unknown-ownership counts without deleting registrations.

#### Scenario: Stale local record
- **WHEN** a discovery record refers to a terminated or replaced instance
- **THEN** connection fails without selecting another process or admitting a mutation

#### Scenario: Unsupported provider
- **WHEN** a client supplies an unregistered transport scheme
- **THEN** it receives a structured unsupported-transport failure

#### Scenario: Exact lookup under saturation
- **WHEN** more candidates or directory entries exist than enumeration permits and a valid known instance is selected
- **THEN** probe, connect and CLI attachment resolve that instance directly and still verify admission and identity

#### Scenario: Bounded enumeration
- **WHEN** enumeration stops with unexamined entries because a budget was reached
- **THEN** its result reports truncation and the corresponding limit reason rather than implying completeness

### Requirement: Bounded portable communication protocol
All production transports SHALL share bounded length framing, UTF-8 JSON encoding, request correlation and version negotiation. Framing SHALL not depend on native message boundaries or C++ ABI. Queue, connection, input and output limits SHALL prevent an unresponsive peer from blocking Main or allocating unbounded memory.

#### Scenario: Fragmentation and malformed length
- **WHEN** frames are fragmented/coalesced or contain an excessive length
- **THEN** valid frames are reconstructed and excessive frames are rejected before allocating the advertised payload

### Requirement: Explicit local admission and future authentication boundary
The Windows provider SHALL restrict attachment to the current local user and reject remote pipe clients. Verified peer facts and connection admission policy SHALL be separate from self-reported target metadata. This release SHALL not open a TCP/UDP listener or advertise unimplemented remote support.

#### Scenario: Disabled or unauthorized attachment
- **WHEN** local automation is disabled or a peer fails the access policy
- **THEN** no automation session is admitted and ordinary application behavior remains usable

### Requirement: Conservative local registration ownership
New local registrations SHALL contain private versioned process ID and process creation identity metadata without changing public target or handshake schemas. Readers SHALL accept legacy target records as unknown ownership. Alive, dead and unknown ownership SHALL be distinct; only a proven dead or replaced process identity permits filtering and opportunistic cleanup. Access/query errors, absent transport endpoints and handshake timeouts SHALL NOT prove death. Invalid, unsupported, legacy or changed records SHALL NOT be automatically deleted. Cleanup SHALL be bounded and SHALL delete only the unchanged file instance checked for removal.

#### Scenario: Process identity reused
- **WHEN** the recorded PID is alive with a different creation identity
- **THEN** the registration is treated as stale for its original process

#### Scenario: Ownership cannot be established
- **WHEN** a record is legacy or process information cannot be queried
- **THEN** valid target data remains an advisory candidate and its file is retained

#### Scenario: Concurrent record replacement
- **WHEN** a stale record changes before cleanup obtains exclusive write/delete exclusion
- **THEN** cleanup retains the changed record

#### Scenario: No live-process false death
- **WHEN** a live target fails or times out during a probe
- **THEN** discovery does not delete its registration on that basis

### Requirement: Validated incoming automation responses

Connection management SHALL use the shared Automation response contract before exposing a peer reply or accepting a completed handshake. Malformed responses SHALL fail outstanding work through the existing connection-failure path; no automatic retry, mutation replay or target fallback SHALL occur. Correlation, framing, admission, timeouts and shutdown draining SHALL remain transport-owned and compatible.

#### Scenario: Malformed handshake
- **WHEN** a peer returns a structurally invalid or non-completed successful handshake envelope
- **THEN** no usable connection is published and the caller receives a controlled protocol failure

#### Scenario: Malformed routed response
- **WHEN** a correlated peer reply has invalid response structure
- **THEN** pending callers receive failure and the connection closes without replay or standalone fallback

#### Scenario: Valid fragmented replies
- **WHEN** valid bootstrap or operation results arrive over a fragmented transport
- **THEN** response data and original framing/correlation behavior remain unchanged
