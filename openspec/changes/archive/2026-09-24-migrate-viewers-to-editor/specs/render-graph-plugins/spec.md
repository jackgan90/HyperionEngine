## MODIFIED Requirements

### Requirement: Configurable rendering plugins
The engine SHALL activate rendering plugins using configuration IDs and use engine wrappers for their math, shaders and graphics work. Scene objects supplied by Editor and Triangle SHALL enter through the generic render primitive registration and collection path. Plugin Main, Render and RHI responsibilities SHALL have explicit execution domains. GUI and non-scene work SHALL retain the standard pass extension path. Scene input, update and status routing SHALL use shared domain interfaces rather than concrete application casts.

#### Scenario: Plugin disabled
- **WHEN** an independent graphics consumer submits no scene or overlay passes
- **THEN** the graph presents its configured clear color

#### Scenario: Two geometry producers
- **WHEN** Editor or Triangle supplies scene geometry
- **THEN** the same Runtime primitive protocol handles registration, collection and removal without an application-specific submission dependency

#### Scenario: GUI overlay
- **WHEN** GUI is enabled over scene geometry
- **THEN** its owned draw data renders in its existing overlay pass with clipping and ordering preserved

#### Scenario: Scene update time
- **WHEN** a scene producer advances simulation or navigation
- **THEN** it receives host delta time independently of presentation and owns that advancement

### Requirement: Centralized RHI frame coordination
The ordinary Editor frame path SHALL send one owned frame job from Render to the RHI coordinator for deferred packet/GUI preparation, graph validation, frame acquisition, recording and submission. RHI 0 SHALL execute its own recording work inline and join every other admitted executor before submission or cancellation. The Render caller SHALL wait at one frame execution boundary.

#### Scenario: Multi-view scene with GUI
- **WHEN** the Editor renders shadow views, forward geometry and GUI
- **THEN** preparation and submission use one Render-to-RHI boundary, command lists preserve graph order, and GUI clipping/visibility remain correct

#### Scenario: Single RHI executor
- **WHEN** concurrent recording is unavailable or only one RHI executor exists
- **THEN** all recording runs inline on the coordinator with no self-queue wait

#### Scenario: Deferred preparation or peer recording fails
- **WHEN** deferred preparation, recording dispatch, a peer recorder or EndFrame fails
- **THEN** all admitted peers finish before active-frame cancellation and the next valid frame can render
