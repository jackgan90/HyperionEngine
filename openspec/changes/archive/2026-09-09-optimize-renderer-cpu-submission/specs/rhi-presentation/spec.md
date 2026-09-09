## ADDED Requirements

### Requirement: Fence-safe native recording reuse
The D3D12 backend SHALL reuse recording storage under the existing frame-slot fence and recording-context ownership rules. State caches SHALL be reset for each command list and suppress redundant setters only when complete effective state matches. Resource/range validation and cancellation, failed-Present and device-failure recovery SHALL remain intact. Recorded logical work and retained packets SHALL remain immutable for all CPU/GPU consumers.

#### Scenario: Repeated frame slots
- **WHEN** a completed slot is reused for compatible prepared draws
- **THEN** native recording reuses storage with fresh list state while previously retained logical results remain unchanged

#### Scenario: State changes within one list
- **WHEN** adjacent draws change PSO, topology, dynamic state or geometry views
- **THEN** the changed state is emitted before its draw and unchanged state is safely reused

#### Scenario: Cancelled or failed presentation
- **WHEN** recording is cancelled or Present fails after submission
- **THEN** submitted resources remain retained through completion and storage is not reset prematurely
