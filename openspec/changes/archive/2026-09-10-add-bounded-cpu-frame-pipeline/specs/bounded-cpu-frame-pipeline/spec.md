## ADDED Requirements

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
