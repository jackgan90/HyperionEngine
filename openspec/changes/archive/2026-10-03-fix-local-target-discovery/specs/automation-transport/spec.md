## MODIFIED Requirements

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

## ADDED Requirements

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
