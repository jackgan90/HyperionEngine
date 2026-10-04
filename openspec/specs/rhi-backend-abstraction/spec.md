# rhi-backend-abstraction Specification

## Purpose
Define extensible, vendor-free RHI backend, device, swapchain and resource contracts with explicit capabilities and safe ownership.
## Requirements
### Requirement: Explicit backend factory and abstract device
The engine SHALL create devices through registered backend providers and expose abstract, vendor-free device interfaces with virtual destruction. The common RHI target SHALL compile and link without a native graphics backend.

#### Scenario: Independently supplied backend
- **WHEN** an application registers a provider implementing the common contracts
- **THEN** the factory creates its device and consumers use the abstract interface without changing common RHI code

#### Scenario: Unavailable backend
- **WHEN** an unknown or unregistered backend is requested
- **THEN** creation fails with an explicit diagnostic instead of choosing another backend

### Requirement: Separate device and presentation lifetimes
Device creation SHALL work without a native window. Swapchain interfaces SHALL separately own window-frame recording and presentation while preserving fenced resource retention.

#### Scenario: Headless resource use
- **WHEN** a D3D12 device is created without a surface
- **THEN** resource creation, capability queries and idle waiting work before any swapchain exists

#### Scenario: Window rendering after separation
- **WHEN** a swapchain is created and used by the renderer
- **THEN** triangle, GUI, resize, readback and shutdown validation continue to pass

### Requirement: Capabilities and shader requirements
The device SHALL expose engine-owned capabilities, distinguish supported features from enabled features, and supply its shader target. Device creation SHALL reject required features that cannot be enabled, and plugins SHALL avoid hard-coded native shader targets.

#### Scenario: Unsupported required feature
- **WHEN** creation requests a feature unavailable in the backend's current implementation
- **THEN** creation fails clearly even if the underlying hardware reports that feature

### Requirement: Resource and submission ownership
Backend implementations SHALL reject foreign resource payloads or resources belonging to another device before issuing draw commands. Swapchains SHALL reject stale or foreign recorded lists before submission.

#### Scenario: Foreign resource
- **WHEN** a packet uses a buffer, texture or pipeline from another device
- **THEN** recording fails with an ownership diagnostic without submitting the invalid packet

### Requirement: Coordinator retirement of renderer-managed resources
Renderer-managed resource ownership SHALL route final underlying resource destruction through RHI 0. The resource coordinator SHALL retain authoritative ownership until admitted CPU operations, draw references and required upload/draw GPU work no longer use the resource. Render primitive destruction or release of a Render-side lease SHALL not by itself destroy a native payload on Render. Ordinary retirement SHALL not require a global WaitIdle.

Completion and retirement processing SHALL make progress without requiring a new presented frame or frame-ring reuse.

#### Scenario: Resource never drawn
- **WHEN** a Renderer-managed resource is created and its last client releases it before any draw submission
- **THEN** the resource is retired on RHI 0 after relevant creation/upload work completes

#### Scenario: Primitive removed during pending GPU work
- **WHEN** a primitive is destroyed while an upload or submitted frame using its resources is fence-gated
- **THEN** the proxy is destroyed on Render, the required resources remain valid until the gate completes, and final underlying destruction occurs on RHI 0

#### Scenario: Frame submission fails
- **WHEN** recording or frame ending fails after resources were admitted or submitted
- **THEN** existing join/cancellation and GPU-retention rules remain in force before the affected resources can be retired

#### Scenario: Retirement after the last presented frame
- **WHEN** the final resource user is removed, pending GPU work completes and no additional frame is submitted
- **THEN** completed retention records and eligible resources are collected on RHI 0 without waiting for another presentation

### Requirement: Resource service shutdown and RHI compatibility
Renderer resource services SHALL drain accepted operations and retirement records before closing the required coordinator executor and device services. They SHALL NOT enqueue cleanup into a closed task system. The new managed-resource path SHALL preserve existing foreign-resource validation, recorded-list identity checks and the independent public RHI behavior in which payload ownership can extend underlying device-state lifetime. Direct RHI clients SHALL retain normal frame-progress collection of completed submissions without requiring a Renderer resource service or new explicit collection calls.

#### Scenario: Shutdown with retained frame data
- **WHEN** rendering shutdown starts while a frame or upload retains a managed resource
- **THEN** the service completes or safely cancels the work, observes required fences and releases retained resources before closing its executors

#### Scenario: Direct RHI ownership remains valid
- **WHEN** existing standalone RHI tests release an IRHIDevice owner while permitted payload or swapchain owners remain
- **THEN** their documented underlying device-state lifetime remains valid independently of the new Renderer service

#### Scenario: Direct RHI frame progress
- **WHEN** a standalone RHI client continues submitting frames and earlier submissions have completed
- **THEN** normal frame progress reclaims completed retention records instead of accumulating them until explicit collection or shutdown

### Requirement: Explicit color formats and multiple targets
RHI SHALL expose vendor-independent color formats, sampled render-color creation, immutable texture format information and enabled format/MRT capabilities. Graphics pipeline target signatures SHALL describe each ordered color slot. D3D12 SHALL bind and validate all declared RTVs, reflect pixel-output compatibility, and retain existing ownership and failure checks. Complete target signatures SHALL participate in pipeline and draw-cache equality.

#### Scenario: Mixed GBuffer formats
- **WHEN** one pass writes RGBA8 and RGBA16F attachments
- **THEN** the backend creates matching views/PSO formats and accepts compatible shader outputs without converting data to sRGB

#### Scenario: Mismatched pipeline or foreign target
- **WHEN** a draw declares the wrong target format/count or an attachment belongs to another device
- **THEN** validation rejects it before GPU submission

### Requirement: Equivalent ordinary and cached native draw interpretation

D3D12 ordinary recording and cached draw-plan construction SHALL share native geometry, dynamic and indexed-draw value interpretation. Cached payloads SHALL express their operation-specific values and bind statistics by named typed fields without relying on generic positional integer arrays or void pointer recovery. Existing supported format, signed offset, first index, counts, stride and dynamic-state semantics SHALL remain unchanged; native details SHALL stay private to the backend.

#### Scenario: Nondefault indexed draw parameters
- **WHEN** a valid packet has nonzero FirstIndex, a legal negative VertexOffset and a nondefault InstanceCount
- **THEN** ordinary, first owned and reused owned paths submit the same independently expected native draw arguments and rendered output

#### Scenario: State switching
- **WHEN** a pass switches pipeline, root/resource bindings, geometry, scissor, blend and stencil state
- **THEN** both paths preserve expected state changes and bind suppression with identical output and named statistics

### Requirement: Cached draw admission preserves ownership and validation

Draw-plan refactoring SHALL preserve shell, target, access and ownership validation, first-miss ordinary recording, later plan construction/reuse, constant-page registration before validation and existing fenced recorded-command retention. Plan caches SHALL NOT become strong resource owners. New immutable streams or changed target/access contracts SHALL invalidate incompatible reuse while previously recorded streams remain valid.

#### Scenario: Cold build and reuse
- **WHEN** an immutable shared draw stream is recorded repeatedly with compatible targets and accesses
- **THEN** its first miss uses validated ordinary recording, its next matching recording builds a plan and later recordings reuse that plan without rebuilding or fully revalidating every draw

#### Scenario: Nested constant page and expired stream
- **WHEN** a stream holds the sole nested constant-page handle and later all commands and fenced work are retired
- **THEN** reset remains blocked while commands are live and becomes possible after they expire, independently of cache lifetime

#### Scenario: Incompatible shell or new stream
- **WHEN** a cached shared stream is submitted through malformed shell storage or incompatible target/access data, or draw arguments change in a new stream
- **THEN** invalid submissions are rejected before execution and the new stream obtains its own validated preparation without changing old immutable commands

### Requirement: Measured native draw-plan maintenance

Changes to shared draw interpretation or cached payload representation SHALL have comparable Release baseline and final measurements for ordinary and owned paths, homogeneous and real state-switch workloads, command storage and cold/build/reuse behavior. Measurements SHALL preserve fixed settings, multiple trials and raw evidence. Repeatable performance or storage regressions SHALL be investigated and resolved or explicitly left unaccepted; a single timing result SHALL NOT establish equivalence.

#### Scenario: Payload refactoring acceptance
- **WHEN** a cached command representation is changed
- **THEN** review receives before/after command size/count/capacity, lifecycle evidence and median/p95 recording times with trial variation for 0, 1, 100, 300, 600 and 1200 draws

### Requirement: Shared typed shader binding contracts

Shader stages SHALL have an engine-owned typed set with explicit graphics-set checks and RHI visibility conversion. Renderer and native providers SHALL reuse one RHI-owned reflected-resource translation and shader/layout consistency implementation. Existing supported stages, encodings, resource kinds and ownership behavior SHALL remain unchanged. Native capability and descriptor validation SHALL remain backend-owned.

#### Scenario: Graphics and compute stage interpretation
- **WHEN** a binding is vertex-only, pixel-only, shared graphics or compute-only
- **THEN** every consumer interprets its existing stage participation consistently and graphics merging rejects empty, unknown or compute-containing sets

#### Scenario: Backend-independent reflection coverage
- **WHEN** a public pipeline binding contract has missing register-space coverage, wrong comparison sampler, insufficient constant range, incompatible structured stride or invalid instance layout
- **THEN** common validation rejects it without requiring a native backend

#### Scenario: Preserved native admission
- **WHEN** existing graphics or compute pipelines are created and rendered through D3D12
- **THEN** supported inputs and output remain compatible and unsupported dimensions, scalars, layouts and device restrictions remain rejected

### Requirement: Opaque pass timing metadata
RHI commands and completed GPU pass timing results SHALL carry caller-owned opaque value metadata without depending on Renderer categories, parsing display labels or borrowing caller memory. Native backends SHALL preserve the metadata separately for every logical pass, including batched native recording and delayed fence completion. Default metadata SHALL remain valid. Timing enablement, cancellation, capture epoch isolation, capacity handling and exactly-once collection SHALL retain their existing behavior.

#### Scenario: Batched tagged commands
- **WHEN** multiple logical commands with distinct tags share native recording and their submission completes
- **THEN** each published pass timing contains the original tag and display name with its own duration

#### Scenario: Delayed completion
- **WHEN** callers release command references after submission and GPU completion is delayed
- **THEN** retained immutable command ownership preserves metadata until timing collection

#### Scenario: Untimed or cancelled work
- **WHEN** timing is disabled or recorded work is cancelled before submission
- **THEN** no timing sample is fabricated and tags do not change rendering or cleanup behavior
