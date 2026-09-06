## Why

The Viewer still renders a hardcoded triangle and lacks depth, camera and material bindings. Loaded CPU assets need a nonblocking GPU publication path and a usable model view.

## What Changes

- Extend public RHI and D3D12 with depth, material bindings, sampler/mip/color-space handling and upload completion.
- Add Renderer model preparation and a ModelViewer plugin using owned CPU snapshots and RHI resources.
- Render static metallic-roughness materials, normal/occlusion/emissive maps, alpha modes and double-sided surfaces.
- Add Viewer model selection, orbit/zoom camera, fit-to-model and visible load/error state while retaining existing triangle workflows.

## Capabilities

### New Capabilities
- `static-model-rendering`: The Viewer still renders a hardcoded triangle and lacks depth, camera and material bindings. Loaded CPU assets need a nonblocking GPU publication path and a usable model view.

### Modified Capabilities
None. Existing configuration, triangle and task behavior is preserved.

## Impact

RHI, D3D12, Renderer, ModelViewer plugin, Viewer, shaders and GPU tests. Depends on add-async-gltf-import.
