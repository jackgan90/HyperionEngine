## MODIFIED Requirements

### Requirement: Early runtime activation and safe lifetime
The plugin SHALL initialize before any DXGI/D3D12 device creation through the shared static plugin lifecycle graph, reuse an injected runtime where present, negotiate a supported API, and retain graphics hooks for the process lifetime. Graphics SHALL NOT require capture when capture is unselected or unavailable.

#### Scenario: Direct Viewer startup
- **WHEN** the plugin is enabled with a compatible installed runtime
- **THEN** the ordinary Viewer process can capture without being launched from the RenderDoc UI

#### Scenario: Unavailable runtime
- **WHEN** the configured runtime cannot load or negotiate the required API
- **THEN** rendering continues and the capture controls report why capture is unavailable

#### Scenario: Compiled provider absent
- **WHEN** ordinary Viewer configuration requests renderdoc in a build without that provider
- **THEN** plugin diagnostics report unavailability and unrelated rendering continues
