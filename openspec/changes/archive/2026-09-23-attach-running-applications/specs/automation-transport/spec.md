## ADDED Requirements

### Requirement: Replaceable asynchronous byte transports
The engine SHALL expose native-independent connection, listener and provider interfaces for reliable ordered full-duplex bytes. Implementations SHALL support bounded asynchronous progress, partial reads/writes, EOF/errors and close without blocking Main or borrowing ephemeral request buffers. A provider registry SHALL select explicitly registered schemes without domain branches.

#### Scenario: Substitute transport
- **WHEN** equivalent byte sequences arrive through Windows pipes or the memory test provider with different fragment boundaries
- **THEN** the shared protocol produces equivalent requests and results without provider-specific dispatch

#### Scenario: Close pending IO
- **WHEN** a connection or listener closes while native IO is pending
- **THEN** admission stops and pending state is cancelled or completed before its storage is destroyed

### Requirement: Independent target discovery and identity
Discovery SHALL return advisory target descriptors with boot-specific instance identity and transport addresses. Direct address connection SHALL not require local discovery or a local PID. The handshake SHALL verify expected identity and compatible communication versions before accepting calls.

#### Scenario: Stale local record
- **WHEN** a discovery record refers to a terminated or replaced instance
- **THEN** connection fails without selecting another process or admitting a mutation

#### Scenario: Unsupported provider
- **WHEN** a client supplies an unregistered transport scheme
- **THEN** it receives a structured unsupported-transport failure

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
