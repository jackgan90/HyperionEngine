## Why

SceneViewer currently owns reusable scene loading, instance bookkeeping, synchronization and shutdown alongside viewer controls. RenderGraph exposes implicit swapchain targets and depth-specific flags rather than explicit attachment, content lifetime and resource dependency contracts. Both prevent reuse and make behavior-preserving changes unnecessarily fragile.

## What Changes

- Introduce a Renderer-owned `FSceneInstance` that composes the existing CPU `FScene` and render bridge, owns asynchronous scene loading and exposes model operations, Tick, structured status and Close without a plugin or GUI dependency.
- Keep SceneViewer as an input/camera/diagnostics adapter; preserve SceneViewer and ModelViewer CLI, configuration, loading, rendering, controls and observable behavior.
- **BREAKING** Replace graph authoring and RHI pass target flags with explicit resource handles, attachment load/store operations, sampled reads and compiled attachment bindings. Migrate every owned caller and test together.
- Separate shadow/forward pass declarations from view collection and deferred draw preparation. Derive ordering and barriers from declared resource accesses, with stable ordering and explicit dependencies where required.
- Preserve resource retention, material/batch caches, independent shadow culling, threaded recording and failed-frame recovery. Support only existing zero/one color and D32/D32S8 attachment use cases; no new rendering algorithms.

## Capabilities

### New Capabilities
- `scene-runtime-instance`: Plugin-independent Renderer scene lifecycle and model ownership facade.

### Modified Capabilities
- `render-graph-plugins`: Explicit graph resources, attachments, content validity, dependencies and owned deferred draw preparation.
- `forward-render-pipeline`: Explicit shadow and forward target/read declarations independent of view data.
- `scene-viewer`: Delegate scene lifecycle to the reusable runtime entity while retaining current controls and diagnostics.
- `rhi-presentation`: Execute resolved attachment bindings and load/store operations without implicit target selection.

## Impact

Runtime Renderer, RHI, Scene integration, D3D12 backend, SceneViewer/DebugUI and other graph callers, Viewer assembly, relevant tests and architecture documentation. Renderer gains a direct dependency on the engine Assets service for asynchronous scene loading; CPU Scene remains independent of Renderer/RHI. Existing executable/target names, serialized schemas, plugin IDs and CLI flags remain stable. Validation includes Debug/Release builds and CTest, style/naming/boundary checks, targeted lifecycle and graph negative tests, and current scene/model viewer acceptance plus rendering/performance comparisons.
