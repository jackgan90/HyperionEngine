## ADDED Requirements

### Requirement: Centralized RHI frame coordination
The ordinary Viewer frame path SHALL send one owned frame job from Render to the RHI coordinator for deferred packet/GUI preparation, graph validation, frame acquisition, recording and submission. RHI 0 SHALL execute its own recording work inline and join every other admitted executor before submission or cancellation. The Render caller SHALL wait at one frame execution boundary.

#### Scenario: Multi-view scene with GUI
- **WHEN** the Viewer renders shadow views, forward geometry and GUI
- **THEN** preparation and submission use one Render-to-RHI boundary, command lists preserve graph order, and GUI clipping/visibility remain correct

#### Scenario: Single RHI executor
- **WHEN** concurrent recording is unavailable or only one RHI executor exists
- **THEN** all recording runs inline on the coordinator with no self-queue wait

#### Scenario: Deferred preparation or peer recording fails
- **WHEN** deferred preparation, recording dispatch, a peer recorder or EndFrame fails
- **THEN** all admitted peers finish before active-frame cancellation and the next valid frame can render

### Requirement: Ordered owned deferred preparation
Graph preparation entries SHALL own their captured frame data, expand in graph order, and preserve dependency validation. Render primitives and Main mutable objects SHALL NOT be read by RHI preparation. Existing immediate graph construction SHALL remain supported.

#### Scenario: Mixed immediate and deferred passes
- **WHEN** immediate passes and owned deferred preparation entries are interleaved
- **THEN** their expanded pass order and explicit dependency constraints remain valid and invalid graphs fail before frame acquisition
