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
