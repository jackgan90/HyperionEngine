## Context

The Viewer still renders a hardcoded triangle and lacks depth, camera and material bindings. Loaded CPU assets need a nonblocking GPU publication path and a usable model view.

## Goals / Non-Goals

Goals: Extend public RHI and D3D12 with depth, material bindings, sampler/mip/color-space handling and upload completion. Add Renderer model preparation and a ModelViewer plugin using owned CPU snapshots and RHI resources. Render static metallic-roughness materials, normal/occlusion/emissive maps, alpha modes and double-sided surfaces. Add Viewer model selection, orbit/zoom camera, fit-to-model and visible load/error state while retaining existing triangle workflows.

Non-goals: no implementation of future FBX/OBJ/COLLADA codecs, animation, compressed glTF extensions, full asset cooking, ECS or additional graphics backends in this series.

## Decisions

### 1. Decision

Preserve existing triangle and GUI draw defaults. Extend RHI descriptors with depth, culling, materials, multiple textures/samplers and explicit texture encoding/mips. Keep native D3D12 structs private. Add only color-plus-depth graph behavior needed by static models, not a general resource graph.

### 2. Decision

Prepare packed vertices, tangents and image mip chains on Worker. RHI 0 creates resources and submits batched uploads, retaining staging resources until a fence completes. Publish the render model only after upload completion; do not wait idle for each model texture.

### 3. Decision

Renderer owns prepared/render model data and material draw packets; ModelViewer is an experiment plugin. Immutable CPU asset results are checked/published at frame boundaries. Default lighting is a documented directional light plus modest ambient, not a promise of IBL/reference-viewer pixel identity.

### 4. Decision

Implement metallic-roughness BRDF with base color, normal, occlusion and emissive roles, correct sRGB sampling/output, UV set/sampler selection, mip filtering, opaque/mask depth writes, sorted blend draws and double-sided normals. Basic fallback textures avoid conditional missing descriptors.

### 5. Decision

Viewer accepts --model and reflected model-source configuration; orbit/zoom and fit operate on transformed model bounds. Loading/error status remains visible and input/frame production continues during delayed IO. Existing bounded capture and triangle modes remain functional.

## Risks / Trade-offs

Resource uploads can still hitch when large CPU copies are performed on RHI; prepare CPU bytes on workers and batch submissions. A single graphics queue is retained. Transparency sorts primitives, not individual intersecting triangles; document this common limitation.

## Migration Plan

Implement after `add-async-gltf-import`. Keep existing target names, serialized keys, triangle and configuration tests working. Each change is additive until its replacement paths are verified; retain explicit compatibility wrappers where required. Validate focused tests before continuing and run full Debug/Release verification at the end.

## Open Questions

No blocking product decisions. Implementation refinements must be reflected here and validated before task completion.
