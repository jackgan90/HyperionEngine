## Why
Rendering plugins need reproducible shader artifacts and resource metadata without direct compiler dependencies.
## What Changes
- Introduce an engine shader compiler wrapper using pinned DXC and SPIRV-Cross.
- Compile HLSL to DXIL, SPIR-V and MSL source, with reflection and content-addressed caching.
## Capabilities
### New Capabilities
- `shader-pipeline`: Shader compilation, reflection, diagnostics and cache invalidation.
### Modified Capabilities
None.
## Impact
Private shader adapter, shader fixtures, cache directory and compiler runtime deployment.
