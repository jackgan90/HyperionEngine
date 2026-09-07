## MODIFIED Requirements

### Requirement: Device-scoped immutable resource sharing
The Renderer SHALL share compatible geometry and material resources among independent instances on the same device. Resource identity SHALL include immutable asset identity/version, subresource identity and relevant preparation/layout configuration. Different devices or incompatible representations SHALL not reuse the same GPU payload. Shared resource identity SHALL not be derived solely from a reusable object address or source filename. Geometry and material resources SHALL have independent leases/identities so unrelated assets can share materials, textures, binding layouts and structurally compatible PSOs. Material-only updates SHALL not reproduce geometry uploads. Unused cache records SHALL be reclaimable after CPU/GPU users finish.

#### Scenario: Two instances of the same asset
- **WHEN** two models use the same immutable asset and compatible preparation on one device
- **THEN** corresponding sections reuse geometry buffers and compatible material resources without duplicate per-instance geometry uploads

#### Scenario: Incompatible resource request
- **WHEN** requests differ in device, geometry layout, texture color-space role or asset version
- **THEN** incompatible resources remain distinct

#### Scenario: Material replacement on shared geometry
- **WHEN** one instance selects a new material or texture revision
- **THEN** unchanged geometry remains shared, the new material is independently prepared and unrelated material users keep their state

### Requirement: Explicit resource readiness publication
Worker preparation, RHI resource creation/upload and Render readiness publication SHALL have explicit domain boundaries. Render SHALL use published completion data instead of calling device resource methods. An item SHALL not reference resources before the necessary upload completion is established. Initial model publication SHALL wait for its required resource group; replacement versions SHALL be adopted only as complete ready versions. Frame-independent ResourcesReady SHALL include selected material interfaces and required static resources/available resource groups as well as geometry, but SHALL NOT depend on View/Object/Draw slices, a target-dependent PSO or final frame bindings. Legacy IsReady SHALL mean ResourcesReady and remain reachable before the first Build. A selected pending static replacement SHALL suspend the affected model group until ready; existing frozen frames SHALL keep their old leases. Failed or superseded static versions SHALL publish stable errors or remain ignored respectively, without invalidating recovery/removal.

#### Scenario: Pending initial model resources
- **WHEN** only some required uploads for a model are complete
- **THEN** the model remains in loading state and no partial resource group is submitted for that model

#### Scenario: Pending replacement
- **WHEN** an already ready instance receives a newer resource version whose upload is pending
- **THEN** no frame mixes incompatible old and new resource state or references the incomplete upload

#### Scenario: Material failure after geometry readiness
- **WHEN** geometry is ready but frame-independent material interface or static binding preparation fails
- **THEN** the affected group reports that static material failure and remains removable or replaceable without submitting partial draws

#### Scenario: First frame readiness
- **WHEN** a material needs camera and object data but no frame has yet been submitted
- **THEN** its static resource group can reach ResourcesReady and the first Build can collect it before constructing frame bindings

## ADDED Requirements

### Requirement: Group-scoped frame binding preparation
After collecting resource-ready items, Renderer/RHI preparation SHALL stage every required packet for a model group and publish draws only if that group's current view/pass preparation succeeds completely. Frame provider, target/PSO or allocation failure SHALL publish a revision/frame/view/usage-qualified draw diagnostic without permanently failing shared static resources. A later valid context SHALL retry and clear its applicable error on success; stale results SHALL not overwrite newer selections. Constant-only changes SHALL not introduce a resource readiness cycle.

#### Scenario: One section has a missing view provider
- **WHEN** one section in a resource-ready model cannot resolve an active frame parameter
- **THEN** that model group emits no partial packets, unrelated valid groups render, and the resource-ready model can recover on a later context with the provider present

#### Scenario: Failure from an old frame
- **WHEN** an old draw preparation result arrives after the selected material revision or a newer applicable frame result changed
- **THEN** it cannot replace current status or block the newer valid material

### Requirement: Material resource retirement integration
Compiled materials, layouts, samplers, binding sets, constant pages and their dependencies SHALL join existing coordinator ownership, asynchronous failure and no-new-frame retirement paths. Releasing one shared user SHALL not cancel other users. Descriptor allocation failure after upload submission SHALL not discard required upload/fence retention.

#### Scenario: New binding resources outlive primitives
- **WHEN** all primitives using a material are removed while a recorded or submitted frame retains its bindings
- **THEN** the bindings remain valid and final managed native destruction occurs on RHI 0 after required completion, without ordinary WaitIdle
