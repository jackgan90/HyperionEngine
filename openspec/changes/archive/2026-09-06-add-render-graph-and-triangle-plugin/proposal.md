## Why
A rendering research framework needs composable passes and a real algorithm plugin to exercise the RHI and execution domains together.
## What Changes
- Add a validated first render graph over the imported swapchain color target.
- Record independent command contexts on indexed RHI threads and submit in graph order.
- Implement a configuration-selected triangle plugin using engine math, shaders and RHI resources.
## Capabilities
### New Capabilities
- `render-graph-plugins`: Validated color-pass graph, plugin contributions and threaded execution.
### Modified Capabilities
None.
## Impact
Rendering module, triangle plugin and Viewer integration; graph CPU tests and GPU image verification.
