## ADDED Requirements

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
