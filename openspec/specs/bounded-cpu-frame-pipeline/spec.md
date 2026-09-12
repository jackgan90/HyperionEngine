# bounded-cpu-frame-pipeline Specification

## Purpose
Define bounded, ordered CPU frame overlap between Main, Render and RHI, with owned frame handoffs, coordinated same-frame recording, safe draining and frame-correlated diagnostics while preserving existing GPU fence semantics.
## Requirements
### Requirement: Independent ordered CPU frame limits
The engine SHALL provide startup Main-to-Render and Render-to-RHI lead limits a and b with nonnegative integer validation, a documented finite maximum and defaults of one. Before advancing from frame N to N+1, Main SHALL wait for Render through max(0,N-a), and Render SHALL wait for RHI through max(0,N-b). Frame IDs SHALL follow engine ticks in submission order.

#### Scenario: Zero and mixed limits
- **WHEN** either lead limit is zero
- **THEN** its producer waits for the just-dispatched downstream frame, while a positive limit on the other boundary still permits that boundary's overlap

#### Scenario: Delayed downstream stage
- **WHEN** a downstream stage is deliberately blocked
- **THEN** the producer can perform the allowed frames and cannot enter the next frame beyond its configured lead

#### Scenario: Invalid configuration
- **WHEN** a limit is negative, fractional, malformed or exceeds supported capacity
- **THEN** initialization fails explicitly before frame admission

### Requirement: Coordinated RHI completion
At least one RHI executor SHALL remain required. RHI 0 SHALL join every same-frame recording peer before ordered submission. The RHI frame completion observed by Render SHALL include that join and submission callbacks, and SHALL NOT imply GPU completion.

#### Scenario: Slow or unused recording executor
- **WHEN** one peer is delayed or some executors have no passes this frame
- **THEN** the frame cannot complete before the delayed peer finishes, and unused executors do not prevent completion

### Requirement: Owned frame input and output
Main-to-Render and Render-to-RHI handoffs SHALL own copied/moved values or immutable snapshots and SHALL NOT borrow producer stack variables. Shared mutable structures SHALL have one execution-domain owner or explicit synchronization. Deferred view/pipeline statistics and capture results SHALL remain associated with the originating frame after later frames are built.

#### Scenario: Inputs change while an older frame waits
- **WHEN** Main changes settings/scene inputs or Render builds a newer graph before an older RHI job runs
- **THEN** the older frame uses its original snapshots and publishes its own statistics without races or overwrite

### Requirement: Bounded lifecycle and failure handling
The pipeline SHALL retain bounded in-flight frame state, observe failures, stop new admission after failure and join all admitted work before dependent objects are destroyed. GPU retirement SHALL retain its existing fence semantics.

#### Scenario: Skipped drawing ticks and resize
- **WHEN** a tick is minimized or zero-sized, or a later tick changes window dimensions
- **THEN** skipped ticks advance ordered CPU completion without drawing, and resize occurs safely in submission order

#### Scenario: Preparation or recording failure
- **WHEN** a frame fails in Render, RHI preparation, peer recording or Present
- **THEN** waiting producers are released with the error, peer ownership is joined, and draining does not rely on successful dependent task bodies

#### Scenario: Exit with outstanding frames
- **WHEN** the run ends with queued frames
- **THEN** all admitted frames are drained before final output verification, plugin/session teardown and native resource release

### Requirement: Frame-correlated diagnostics and capture
Viewer SHALL consume completed results on Main, expose CPU stage progress, and correlate benchmark/capture outputs to originating frame IDs. Explicit RenderDoc requests SHALL target their intended frame despite prior queued work. Existing synchronous execution entry points SHALL remain available.

#### Scenario: Async screenshot and benchmark tail
- **WHEN** the final tick requests a screenshot during an asynchronous benchmark
- **THEN** the output is saved after its completion and all benchmark rows use their own frame's draw and device statistics

### Requirement: Scene publication is bound to frame admission
Scene-bound frame input SHALL carry an owned publication token including logical scene identity, attachment epoch and publication serial. Normal Main admission SHALL publish scene updates and enqueue that frame before publishing a later scene state. Render SHALL verify an exact match to its applied publication before resolving camera, lights and geometry for the frame. A mismatched, foreign or failed publication SHALL fail explicitly before scene draw generation rather than substituting the latest available state. The token SHALL NOT require unbounded historical scene retention or synchronous GPU completion.

#### Scenario: Delayed Render with later Main edits
- **WHEN** Render is gated while Main admits frame N and edits/admit frame N+1
- **THEN** Render processes publication N and frame N before publication N+1 and frame N+1, preserving each frame's corresponding model, camera and light data

#### Scenario: Caller queues publications out of the frame contract
- **WHEN** a caller freezes an old token, applies a newer publication and then requests the old scene frame
- **THEN** Render reports ScenePublicationMismatch without rendering a mixture or silently selecting the newer publication

#### Scenario: Reattach the same logical scene
- **WHEN** an old token is reused after the same FScene attaches again
- **THEN** the changed attachment epoch rejects it even if other identifiers or revision values appear equal

### Requirement: Scene control progress and old-frame lifetime remain independent of presentation
Scene camera/light/model updates, metadata removal and failure observation SHALL progress on the ordered control queue during minimized or skipped-drawing ticks. Closing a scene SHALL clear its Render metadata and join admitted CPU work, including after publication failure. Already prepared frames SHALL retain their original immutable data and existing GPU leases, with no new scene-specific GPU idle waits.

#### Scenario: Minimized camera and light edit
- **WHEN** camera/light edits and removals occur while drawing is skipped
- **THEN** the scene publication progresses and the next valid frame reflects those changes without stale selection or lighting

#### Scenario: Close with delayed RHI work
- **WHEN** an old prepared graph is retained while scene nodes are removed or the scene is closed
- **THEN** that graph remains valid through its existing owners and new scene frames do not inherit removed metadata
