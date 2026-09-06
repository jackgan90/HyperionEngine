## Why
The Windows application needs actual GPU resources and presentation through a stable engine contract before algorithm plugins are introduced.
## What Changes
- Add a private D3D12 backend with hardware adapter selection, command contexts, GPU fences, swapchain resize and presentation.
- Integrate D3D12 Memory Allocator for buffers/textures with engine allocation callbacks.
- Add owned draw packets, pipeline descriptions and GPU readback screenshots, ready for graph and GUI passes.
## Capabilities
### New Capabilities
- `rhi-presentation`: Owned graphics resources, parallel recording, presentation and GPU lifetime handling.
### Modified Capabilities
None.
## Impact
New RHI module and Windows Viewer render loop; private D3D12/DXGI/D3D12MA dependencies.
