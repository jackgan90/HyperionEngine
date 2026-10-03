## Why

Shader binding stages are interpreted through repeated numeric masks, and Renderer and D3D12 independently translate reflected resource kinds. Common shader/layout validation is private to D3D12 even though it uses only engine contracts, making equivalent changes require synchronization across layers.

## What Changes

- Introduce an engine-owned typed shader stage set and explicit RHI visibility conversions; migrate graphics merging, compute bindings and per-stage accounting.
- Share reflected resource-kind translation and shader/layout coverage validation in RHI, retaining native capability and descriptor checks in D3D12.
- Preserve existing stages, encodings, supported resources, pipeline behavior, diagnostics and resource ownership.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `rhi-backend-abstraction`: Require shared typed shader binding interpretation and backend-independent shader/layout consistency checks.

## Impact

Shaders, RHI, Renderer and the D3D12 provider; shader/material/RHI tests and architecture documentation. No new backend, shader stage, rendering feature, asset format or performance optimization.
