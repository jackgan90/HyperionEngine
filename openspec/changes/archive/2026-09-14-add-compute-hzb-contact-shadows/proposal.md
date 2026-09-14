## Why

The renderer currently exposes only graphics pipelines, preventing reusable GPU compute workloads. A compute-generated hierarchical depth buffer and Deferred contact shadows provide independently verifiable infrastructure and an actual consumer that restores small directional-light shadow details in SceneViewer Sponza.

## What Changes

- Add portable compute shader compilation/reflection, typed compute pipelines, mutable upper-layer parameters published as immutable dispatch inputs, sampled/storage resources and graph-scheduled dispatch.
- Extend graph resource accesses to mip views and buffers, including initialization, transitions and UAV ordering; separate logical pass count from bounded native recording contexts and preserve per-pass timings.
- Provide an independently requested HZB producer with nearest/farthest reduction semantics for both depth conventions. Generate only requested products and share compatible requests within a frame/view.
- Generate a fullscreen contact visibility mask from current Deferred depth/HZB and combine it with CSM using minimum visibility for the main directional light.
- Expose dynamic controls, mask/HZB diagnostics and activation statistics through DebugUI. Forward and later compatibility/transparent surfaces retain their current shadow behavior.
- Validate compute independently, compare HZB numerically against a CPU reference, and accept the integrated effect on ready Sponza in both depth conventions.

## Capabilities

### New Capabilities

- `compute-pipelines`: Engine-owned compute programs, bindings, dispatch, capabilities and lifetime contracts.
- `hierarchical-depth`: Requested HZB products, precise reduction semantics and mip generation.
- `contact-shadows`: Directional contact visibility, controls, activation and visual/numerical validation.

### Modified Capabilities

- `shader-pipeline`: Compute artifacts, writable resource reflection and reflected thread-group dimensions.
- `render-graph-plugins`: Compute/mip/buffer dependencies, deferred dispatch preparation and bounded recording batches.
- `deferred-render-pipeline`: Optional HZB/contact passes between BasePass and lighting and directional visibility composition.

## Impact

Changes affect Shaders, RHI, D3D12, Renderer, shared resource/parameter helpers, Config, DebugUI, Viewer, engine shaders, tests and documentation. Native APIs remain private; Scene and Environment remain CPU-only. Existing executable/target names and serialized settings remain compatible. Runtime execution initially uses the existing D3D12 direct queue, with no async-compute queue requirement. The authorized delivery includes proposal and implementation but no Git commit.
