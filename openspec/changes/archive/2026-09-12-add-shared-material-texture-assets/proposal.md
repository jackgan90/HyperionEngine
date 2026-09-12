## Why

Native model files currently embed fixed glTF materials and image pixels. Independent models cannot refer to the same editable material or texture, and their preparation creates distinct CPU/GPU resources. The renderer already supports general typed material parameters, shader reflection and multiple passes, but those descriptions cannot be authored as native assets.

## What Changes

- Introduce reflected texture and general material assets. Texture pixels, encoding and offline mip chains become independently addressable native data; materials store shader paths, entry points, defines, passes, typed defaults/values and texture references.
- Change persistent models to geometry, hierarchy and typed material slots. Resolve model/material/texture graphs through the generic native asset service and reuse material definitions, immutable texture sources and existing GPU caches across models.
- Extend offline import to named subassets with stable identities and a shared content library. glTF/GLB import and legacy model upgrade publish complete model/material/texture graphs, track external and embedded sources, and retain transactional root publication.
- Persist scene material selections and typed local overrides. Numeric edits update material state without rebuilding geometry or uploading unchanged textures.
- Update samples, tooling, diagnostics, compatibility tests and documentation. Shader source remains compiled through the existing compiler; binary shader assets are deferred.

## Capabilities

### New Capabilities

- `texture-assets`: Reflected standalone texture data, validation, offline mip generation and shared runtime texture sources.
- `material-assets`: Reflected general material descriptions, shader/pass contracts, texture references and typed persistent values.

### Modified Capabilities

- `static-model-data`: Persistent material slots become typed asset references, with explicit offline upgrade for embedded legacy models.
- `offline-asset-import`: Named subasset products, shared library identities and transactional graph publication.
- `static-model-rendering`: Resolve independent material/texture assets and reuse CPU/GPU resources across models.
- `scene-runtime-instance`: Persist asset-backed material selections and typed overrides for scene instances and sections.
- `gltf-pipeline-validation`: End-to-end validation of shared assets, authored shaders, upgrade, failure recovery and native-only rendering.

## Impact

Touches Scene, Materials, AssetImport, Assets registration, Renderer resource preparation, Viewer, AssetTool, content generation, tests and documentation. A focused CPU-only Textures module owns texture records and mip processing. Scene and Materials remain independent of RHI; source adapters stay private to AssetImport. Existing generic material APIs and GPU lifetime rules remain authoritative. Model schema changes require explicit offline upgrade of old embedded assets. No Git commit is part of this change.
