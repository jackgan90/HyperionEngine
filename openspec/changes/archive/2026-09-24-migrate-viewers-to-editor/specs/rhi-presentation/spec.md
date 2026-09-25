## MODIFIED Requirements

### Requirement: Hardware presentation through RHI
The engine SHALL create a hardware D3D12 device and present to its platform window through vendor-free RHI types.
#### Scenario: Clear frame
- **WHEN** the Windows Editor submits a clear frame
- **THEN** a DX12 backbuffer is presented and a readback image contains the expected clear color.

### Requirement: Diagnostics and teardown
The backend SHALL report the selected adapter, debug-layer availability and GPU validation errors and shut down after draining work.
#### Scenario: Lifecycle smoke test
- **WHEN** a bounded Editor run exercises resize, minimize and restore
- **THEN** it exits successfully with zero reported D3D12 error or corruption messages when the debug layer is available.
