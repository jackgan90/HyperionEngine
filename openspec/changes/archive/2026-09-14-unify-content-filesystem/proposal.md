## Why

Runtime content currently depends on source assets and shader paths inside the engine checkout. Separate engine resources from sample content and make loading independent of checkout location through mounted package paths.

## What Changes

- Add engine-owned mounted file access for `/Engine` and `/Game`, including reads, enumeration, writes and leases.
- Move engine shaders and the shared environment BRDF LUT into `Content`; migrate sample native assets and plain-text sample shaders to HyperionAssets.
- Preserve text Shader formats; no shader hasset format is introduced.
- Make references, scene persistence, catalogs and shader compilation use mounted paths while retaining explicit local-file tool/test support.
- Add reproducible source manifests, local source caches, stable import source identities and external dependency publication.
- **BREAKING**: sample configuration paths change from `out/content` to `/Game`; ordinary Viewer builds no longer import source samples.
- Configure Git LFS for external hasset content and retain provenance/licenses. Both repositories remain uncommitted for acceptance.

## Capabilities

### New Capabilities
- `mounted-content-filesystem`: portable mounted file access and application configuration.
- `external-content-repository`: content ownership, LFS distribution, source recovery and migration acceptance.

### Modified Capabilities
- `native-asset-management`: mounted references and multiple catalogs.
- `offline-asset-import`: external dependencies, portable source identity and explicit content generation.
- `shader-pipeline`: mounted text sources, includes and relocation-independent cache keys.
- `skybox-skylighting`: engine-owned BRDF resource and external native environment distribution.

## Impact

IO, Assets, AssetImport, Shaders, Renderer persistence, Viewer and plugins, CMake, tests and tools, active documentation, engine Content and the separate HyperionAssets working tree. No commits, history rewrite, archive, remote publication, shader format conversion or rendering algorithm changes.
