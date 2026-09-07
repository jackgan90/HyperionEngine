## Why

Main currently owns individual renderer-bound Models, while Render collects every primitive before item-level frustum filtering. A general logical scene and early hierarchical visibility are needed to manage multiple models and avoid unnecessary collection work.

## What Changes

- Introduce a renderer-independent Main scene with flat, versioned model instances and owned change snapshots.
- Separate logical instance data from Main-side render bindings; retain the existing renderer Model adapter for compatible direct clients.
- Add Render-owned model culling groups, a replaceable spatial index, binary AABB BVH and pre-collection frustum visibility.
- Preserve zero-to-many primitive collection, resource sharing, global transparency/depth and GPU retirement.
- Add a scene-viewer plugin using versioned scene manifests, shared assets, multi-instance examples, camera/debug controls and visibility statistics; migrate ModelViewer to the scene path.

## Capabilities

### New Capabilities
- `scene-management`: Logical ownership, stable handles, Main-to-Render synchronization and lifecycle.
- `scene-visibility`: Replaceable spatial indexing, conservative bounds, BVH maintenance and view-local early culling.
- `scene-viewer`: Manifest loading, multiple instances, comparison modes, diagnostics and controls.

### Modified Capabilities
- `render-primitives`: Conservative pre-collection bounds and model-group visibility while preserving collection cardinality.

## Impact

Math, Scene, Renderer, ModelViewer, a new SceneViewer plugin, Viewer composition/configuration, tests, example content and documentation. Existing executable/target names, plugin IDs, CLI flags and serialized model fields remain stable. No new third-party dependency, GPU occlusion implementation, scene parenting or git commit is included.
