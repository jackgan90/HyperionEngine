## ADDED Requirements

### Requirement: Persistent render primitives
The Renderer SHALL represent scene-renderable instances as persistent render primitives, separate from CPU assets, Main-thread logical objects and frame draw packets. Primitive collection SHALL support zero or multiple render items without requiring a fixed correspondence between primitive count and GPU draw count.

#### Scenario: Multiple instances of one geometry
- **WHEN** two logical objects reference the same geometry at different transforms
- **THEN** they have independently addressable primitive state while referencing shared geometry resources

#### Scenario: Collection cardinality
- **WHEN** a registered primitive emits no items or emits multiple sections
- **THEN** the standard collector accepts that result without changing the primitive registration protocol

### Requirement: Render ownership and independent type extension
The Render domain SHALL exclusively construct, register, mutate and destroy fully constructed primitives. A render scene SHALL exclusively own its registered primitives. Main objects SHALL use opaque bindings and SHALL NOT dereference primitive state. Primitive implementations SHALL NOT retain or dereference mutable Main-thread logical objects. Main and Render type hierarchies SHALL be independently extensible through engine-owned interfaces.

#### Scenario: Main wrapper is destroyed
- **WHEN** a Main wrapper is destroyed after its create command was accepted but before Render processes it
- **THEN** accepted work uses owned data, removal completes safely, and every fully constructed primitive is destroyed exactly once on Render

#### Scenario: Independent primitive implementation
- **WHEN** a test supplies a different primitive implementation through the generic creation and collection interface
- **THEN** it renders or collects without a ModelViewer dependency or a matching Main-side inheritance hierarchy

#### Scenario: Wrong-domain access
- **WHEN** a guarded primitive mutation or scene registration is invoked outside Render
- **THEN** the execution-domain contract violation is detected before mutable render state is accessed

### Requirement: Owned cross-domain messages
Creation, update, removal and completion messages SHALL contain values, owned storage or immutable shared snapshots. They SHALL NOT borrow Main objects, mutable container views or caller stack storage. Main-visible status and errors SHALL be published through independent result data without requiring Render to wait for a Main callback.

#### Scenario: Caller changes source data
- **WHEN** Main submits an update and modifies or destroys the original input before Render consumes it
- **THEN** Render observes the submitted snapshot without reading the changed or destroyed input

#### Scenario: Status consumer has disappeared
- **WHEN** a load result arrives after its Main wrapper has been destroyed
- **THEN** result processing completes without calling through that wrapper or blocking on Main

### Requirement: Versioned registration and ordered updates
Primitive handles SHALL distinguish scene identity, slot and generation. Commands SHALL have a defined logical order independent of asynchronous preparation completion. Complete state snapshots SHALL carry monotonic revisions, and resource completions SHALL identify the request version they satisfy. Stale generations, stale revisions and superseded results SHALL NOT modify current state.

#### Scenario: Slot reuse
- **WHEN** a removed primitive's slot is reused and an old update or removal arrives
- **THEN** the new primitive remains unchanged

#### Scenario: Completion order differs from submission order
- **WHEN** an older preparation finishes after a newer state or resource version was selected
- **THEN** the older result cannot replace the newer state

#### Scenario: Foreign scene handle
- **WHEN** a command names a handle from another render scene
- **THEN** it cannot mutate or remove an object in the receiving scene

### Requirement: Atomic frame-boundary updates
The Renderer SHALL define a command-consumption boundary for each frame and apply accepted updates in logical order before freezing that frame's collection state. A related multi-primitive update batch SHALL be validated and published atomically. Commands beyond the boundary SHALL affect a later frame.

#### Scenario: Whole-model transform update
- **WHEN** Main updates a model containing multiple sections in one batch
- **THEN** a frame observes either the previous complete model transform state or the new complete state, never a mixture

#### Scenario: Invalid update batch
- **WHEN** validation of one member of an otherwise applicable batch fails
- **THEN** none of that batch's state changes are published and a diagnostic is returned

#### Scenario: Invalid section in a ready resource batch
- **WHEN** a member of an otherwise applicable update batch references a section outside its already ready resource description
- **THEN** the update reports an error before publishing any member, preserving the previous complete batch state

#### Scenario: Update during collection
- **WHEN** another update is submitted after collection state has been frozen
- **THEN** the current frame remains unchanged and the update is considered at a later boundary

### Requirement: Side-effect-free collection and view input
Primitive collection SHALL read frozen render state and SHALL NOT advance logical state, allocate native GPU resources or directly submit native commands. View and frame settings SHALL enter through owned frame data. Hidden, unready or conservatively proven out-of-frustum primitives SHALL produce no scene draws; unavailable bounds SHALL not justify rejection.

#### Scenario: Repeated collection
- **WHEN** the same state is collected for multiple views or passes
- **THEN** logical state and animation time remain unchanged by collection

#### Scenario: Visibility filtering
- **WHEN** an object is hidden, not ready or wholly outside the view according to valid conservative bounds
- **THEN** it produces no draw, while an object without usable bounds remains eligible for collection

### Requirement: Independent frame ownership
Frame descriptions and admitted RHI work SHALL retain their required data and resource leases independently of primitives. They SHALL NOT borrow primitive memory. Removing a primitive SHALL prevent collection in subsequent frames after removal is applied without invalidating already frozen frames.

#### Scenario: Removal with an existing frame snapshot
- **WHEN** a primitive is removed after a frame has collected it but before that frame finishes
- **THEN** its Render object can be destroyed after CPU borrows end, and the existing frame completes using independently retained data

### Requirement: Idempotent removal and terminal publication
Removing a binding SHALL stop new updates from that binding. Render removal SHALL be idempotent and SHALL prevent later asynchronous results from registering or reviving the same generation. A removal receipt SHALL indicate that Render no longer uses the object, separately from GPU completion.

Control-message progress SHALL remain available when no presentable frame is produced, without mutating an already frozen frame snapshot.

#### Scenario: Remove during upload
- **WHEN** a primitive is removed while its resource upload remains pending
- **THEN** it is unregistered without waiting for unrelated users, and the late upload result only completes or retires the resource request

#### Scenario: Repeated removal
- **WHEN** removal is requested more than once for the same generation
- **THEN** the primitive is destroyed at most once and no other registration is affected

#### Scenario: Removal while presentation is paused
- **WHEN** a primitive is removed while the window is minimized or frame production is paused
- **THEN** the Render control pump completes removal without requiring another BeginFrame or Present

### Requirement: Failure-independent cleanup
Failed construction, preparation, upload or state application SHALL preserve a cleanup path for every accepted registration and resource request. Cleanup SHALL NOT rely on execution of a task whose body is skipped when a prerequisite fails. A failed request SHALL publish an error without preventing later removal or scene shutdown.

#### Scenario: Failed upload followed by removal
- **WHEN** an accepted upload fails before its primitive is removed
- **THEN** the error is reported, removal still executes and retained resources reach their retirement path

#### Scenario: Invalid section discovered after preparation
- **WHEN** a resource description becomes ready after a create or update was accepted and reveals an invalid section reference
- **THEN** the binding reports a failed state without requiring a new frame, invalid items do not prevent other scene items from rendering, and a later valid update or removal remains available without retroactively rolling back the accepted revision

#### Scenario: Construction fails
- **WHEN** Render-side construction or registration throws
- **THEN** partial state is cleaned on its owning domain, no live registration is published, and the handle reaches a terminal failed or removed state

### Requirement: Ordered session shutdown
The rendering session SHALL stop external producers and close client admission before draining accepted work. Internal preparation results and cleanup SHALL be drained while the task system remains available. Render primitives SHALL be destroyed on Render before their resource services are closed; RHI work and GPU resource retirement SHALL finish before the required executors and device services are torn down. An expired Main binding SHALL NOT dispatch into a closed task system.

Shutdown SHALL continue control-message progress independently of frame production and SHALL NOT introduce same-queue waits or cyclic Render/RHI waits.

#### Scenario: Shutdown with pending work
- **WHEN** shutdown begins with queued creation, pending upload and an active frame
- **THEN** all accepted work reaches a terminal result, primitives are removed, submitted work is safely drained and no cleanup is stranded on a stopped executor

#### Scenario: Binding outlives the session
- **WHEN** a Main binding is destroyed after its session has completed shutdown
- **THEN** it only releases invalidated client state without dereferencing the scene or enqueueing work
