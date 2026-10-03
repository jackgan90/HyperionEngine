## ADDED Requirements

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
