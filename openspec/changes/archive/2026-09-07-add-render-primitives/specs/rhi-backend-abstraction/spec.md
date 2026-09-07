## ADDED Requirements

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
