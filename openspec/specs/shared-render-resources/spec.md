# shared-render-resources Specification

## Purpose
Define device-scoped immutable render resource sharing, asynchronous readiness, instance isolation and retirement independent of primitive lifetimes.

## Requirements
### Requirement: Device-scoped immutable resource sharing
The Renderer SHALL share compatible geometry and material resources among independent instances on the same device. Resource identity SHALL include immutable asset identity/version, subresource identity and relevant preparation/layout configuration. Different devices or incompatible representations SHALL not reuse the same GPU payload. Shared resource identity SHALL not be derived solely from a reusable object address or source filename.

#### Scenario: Two instances of the same asset
- **WHEN** two models use the same immutable asset and compatible preparation on one device
- **THEN** corresponding sections reuse geometry buffers and compatible material resources without duplicate per-instance geometry uploads

#### Scenario: Incompatible resource request
- **WHEN** requests differ in device, geometry layout, texture color-space role or asset version
- **THEN** incompatible resources remain distinct

### Requirement: Shared asynchronous production
Compatible concurrent resource requests SHALL share preparation/upload production. Cancelling one requester SHALL not invalidate another live requester. A failed production SHALL publish terminal errors and SHALL allow a later request to retry without reviving removed primitives.

#### Scenario: One requester cancels
- **WHEN** two models await one shared upload and one model is removed
- **THEN** the remaining model can become ready from that upload and the removed model stays removed

#### Scenario: Retry after failure
- **WHEN** a shared production fails and a new request is made for the same asset
- **THEN** the new request can perform a new attempt while previous request errors remain stable

### Requirement: Instance state isolation
Shared geometry and material definitions SHALL remain independent from per-instance transform, visibility and material overrides. Updating one instance SHALL not mutate the frozen state or shared definition observed by another instance. Resource replacements SHALL use explicit versions.

#### Scenario: One model changes
- **WHEN** one of two models sharing an asset changes its transform, visibility or material override
- **THEN** only that model's rendering changes and shared geometry remains reusable

### Requirement: Explicit resource readiness publication
Worker preparation, RHI resource creation/upload and Render readiness publication SHALL have explicit domain boundaries. Render SHALL use published completion data instead of calling device resource methods. An item SHALL not reference resources before the necessary upload completion is established. Initial model publication SHALL wait for its required resource group; replacement versions SHALL be adopted only as complete ready versions.

#### Scenario: Pending initial model resources
- **WHEN** only some required uploads for a model are complete
- **THEN** the model remains in loading state and no partial resource group is submitted for that model

#### Scenario: Pending replacement
- **WHEN** an already ready instance receives a newer resource version whose upload is pending
- **THEN** no frame mixes incompatible old and new resource state or references the incomplete upload

### Requirement: Resource leases independent of proxies
Renderer-managed resources SHALL retain independent ownership for active bindings, accepted uploads and frozen frame work. Releasing the last proxy reference SHALL only request retirement; it SHALL NOT invalidate resources still used by another instance or an admitted operation. Unreferenced resources SHALL be reclaimable during normal operation through coordinator collection without a global GPU idle.

#### Scenario: Remove one shared instance
- **WHEN** one of several primitives sharing geometry is removed
- **THEN** the other primitives continue rendering with valid shared resources

#### Scenario: Last user leaves
- **WHEN** the last instance and frame lease are released and all relevant work is complete
- **THEN** the resource becomes reclaimable through the coordinator during normal operation rather than remaining pinned until application shutdown

### Requirement: Batch-compatible render descriptions
Render descriptions SHALL separate geometry/section identity, layout, material bindings and render state from per-instance parameters. They SHALL preserve sufficient compatibility information to distinguish differing shader variants, resource versions, depth/blend/cull state, mirrored winding and deformation representation. Resource reuse SHALL not be reported as GPU instancing or imply draw merging.

#### Scenario: Shared geometry with incompatible material state
- **WHEN** two items use the same geometry but differ in alpha mode, material binding or winding state
- **THEN** the Renderer preserves those differences even though geometry storage is shared

#### Scenario: Ordinary draw submission
- **WHEN** multiple compatible primitives are rendered without an instancing implementation
- **THEN** separate ordinary draws remain valid and the primitive protocol does not require the proxy count to equal a permanently fixed draw count
