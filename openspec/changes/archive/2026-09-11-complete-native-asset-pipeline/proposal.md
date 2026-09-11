## Why

ModelViewer and SceneViewer still parse glTF and hand-written scene JSON at runtime even though recursive reflection, native archives and an AssetImport module already exist. Making native assets the runtime contract now provides a reusable loading/saving path before more asset types and external formats are introduced.

## What Changes

- Extend existing record reflection with registered type discovery, explicit field compatibility, schema migrations and contextual transactional diagnostics.
- Define a stable native asset container with identity, dependencies, fixed wire encoding, bulk payloads, integrity checks, bounded allocation and legacy archive upgrade support.
- Add lightweight asset reference data, dependency resolution, bounded caching and explicit content revision invalidation to the native asset service.
- Turn AssetImport into an offline conversion service and command-line tool with glTF/GLB and existing scene JSON adapters, source dependency fingerprints, stable output identities and transactional batch publication.
- **BREAKING**: runtime model/scene paths accept native `.hasset` files; external source formats are imported explicitly before launching a Viewer. Configuration keys and Viewer executable/CLI names remain stable.
- Migrate sample content and acceptance fixtures to native assets; retain model sharing, scene layout, rendering and asynchronous lifetime behavior.
- Provide scene snapshot saving for editable instance state and camera, with explicit failure for material selections without a persistent representation.

## Capabilities

### New Capabilities

- `native-asset-management`: native containers, asset identity/references, dependency graphs, bounded cache and revision behavior.
- `offline-asset-import`: importer registration, reproducible source conversion, incremental import records, transactional publication and import tooling.

### Modified Capabilities

- `reflected-object-archives`: type discovery, field metadata, migration chains, stable encoding and contextual validation.
- `async-gltf-import`: move external source loading into the import service while preserving static conversion and request lifecycle guarantees.
- `scene-runtime-instance`: consume reflected native scene assets and expose persistent scene snapshots.
- `scene-viewer`: native scene loading and saving of current editable scene state.
- `gltf-pipeline-validation`: validate the import-to-native-to-render pipeline with runtime source access prohibited.
- `asset-math-foundation`: relocate legacy synchronous glTF conversion into AssetImport.

## Impact

Affected modules are Reflection, Serialization, Assets, Scene, AssetImport, Renderer, Viewer and SceneViewer. A small AssetTypes module and an AssetTool application are added. CMake content generation, fixtures, tests and asset documentation change. cgltf remains private to import adapters; Scene remains independent of Renderer/RHI. Existing GPU readiness, resource ownership and fence retirement remain intact. FBX/OBJ adapters, animation, streaming, hot reload, texture compression and mip baking are future extensions. No Git commit is part of this delivery.
