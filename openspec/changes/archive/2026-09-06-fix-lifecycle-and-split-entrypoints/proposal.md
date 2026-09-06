## Why

Architecture review reproduced invalidation of surviving windows, an unrecoverable active frame after recording failure, and failed asset requests preventing retry. Viewer and glTF import orchestration also exceed readable function sizes; fix this bounded set before further engine features.

## What Changes

- Balance each window's SDL subsystem lifetime without global shutdown of other windows.
- Add explicit RHI frame cancellation and drain recording work before recovering from graph execution failures.
- Retry completed failed asset loads while preserving successful caching, in-flight sharing and consumer cancellation.
- Split Viewer and glTF importer functions by responsibility without changing their supported behavior.
- Document the default 100-line function limit and the tightly coupled logic exception.
- Add focused regression coverage for the three reproduced failures.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `application-platform`: surviving windows remain usable after a peer window is destroyed.
- `rhi-presentation`: cancel an active frame safely and allow subsequent frames.
- `render-graph-plugins`: join recorders and cancel a failed frame before propagating errors.
- `async-gltf-import`: a fresh request retries a completed failed load.
- `code-style`: functions normally stay within 100 lines, with documented tightly coupled exceptions.

## Impact

Platform, RHI/D3D12, Renderer graph execution, AssetService, Viewer, private glTF adapters, regression tests and repository guidance. The swapchain provider interface gains a cancellation operation, requiring the in-tree mock provider to implement it. No changes to serialized IDs, CLI flags, CMake target names, materials, graph resources, reflection architecture or unrelated review findings.
