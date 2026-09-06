## ADDED Requirements

### Requirement: Failed graph frame cleanup
Graph execution SHALL join all admitted recording tasks and cancel its active frame before propagating a dispatch, recording or end-frame error.

#### Scenario: Retry after a recording error
- **WHEN** an invalid draw causes graph recording to fail and a valid graph is then executed
- **THEN** the valid graph renders without an already-active-frame error

#### Scenario: Peer recorder still runs
- **WHEN** one recording fails while another recording is pending
- **THEN** cancellation occurs only after the pending recorder finishes
