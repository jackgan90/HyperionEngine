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
