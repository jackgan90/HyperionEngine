# rhi-presentation Specification

## Purpose
TBD - created by archiving change add-d3d12-rhi-and-presentation. Update Purpose after archive.
## Requirements
### Requirement: Hardware presentation through RHI
The engine SHALL create a hardware D3D12 device and present to its platform window through vendor-free RHI types.
#### Scenario: Clear frame
- **WHEN** the Windows Viewer submits a clear frame
- **THEN** a DX12 backbuffer is presented and a readback image contains the expected clear color.

### Requirement: GPU-safe frame lifetime
The backend SHALL retain submitted resources until the owning GPU fence completes and isolate command allocators between concurrently recorded contexts.
#### Scenario: Reused frame
- **WHEN** a frame context is reused or the window is resized
- **THEN** prior GPU work completes before its allocators or backbuffers are reset or released.

### Requirement: Diagnostics and teardown
The backend SHALL report the selected adapter, debug-layer availability and GPU validation errors and shut down after draining work.
#### Scenario: Lifecycle smoke test
- **WHEN** a bounded Viewer run exercises resize, minimize and restore
- **THEN** it exits successfully with zero reported D3D12 error or corruption messages when the debug layer is available.

### Requirement: Recoverable frame cancellation
RHI SHALL expose idempotent frame cancellation on the coordinator after recording tasks finish. It SHALL retain submitted resources until GPU completion and allow a subsequent frame when cancellation succeeds on a healthy device.

#### Scenario: Cancel an unsubmitted frame
- **WHEN** a begun frame has recorded commands but cannot be submitted
- **THEN** cancellation abandons those commands and the next frame can render normally

#### Scenario: Cancel after submission failure
- **WHEN** frame ending fails after commands were submitted
- **THEN** cancellation drains submitted work before releasing its resources and reports a failure if the device cannot safely recover

#### Scenario: Presentation reports failure while GPU commands remain pending
- **WHEN** Present fails after command submission and successful fence signaling
- **THEN** the frame remains cancellable until submitted work is drained, the original presentation error is reported, and a healthy device can render the next frame

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

### Requirement: Explicit resolved graphics attachments

RHI graphics commands SHALL contain explicit resolved attachment references, view formats and load/store operations. The backend SHALL bind only the declared targets and validate their compatibility with draw pipelines. Backbuffer and frame depth references SHALL be explicit even when they resolve to existing swapchain storage. Graph transitions SHALL identify their target resource, including presentation transitions. Existing recording concurrency, GPU-safe lifetime and failure recovery SHALL remain intact.

#### Scenario: Depth-only recording
- **WHEN** commands declare only a sampled D32 depth attachment
- **THEN** the backend binds that DSV with zero RTVs using its dimensions and performs the declared depth load/store operations

#### Scenario: Forward and overlay
- **WHEN** forward commands declare backbuffer and main depth and GUI declares only backbuffer Load/Store
- **THEN** forward depth and color are initialized as declared and the GUI overlays without clearing prior scene color
