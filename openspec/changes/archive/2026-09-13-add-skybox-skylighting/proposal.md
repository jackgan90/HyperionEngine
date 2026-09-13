## Why

The renderer has only a constant environment color and no visible sky. Scenes need a native HDR environment asset that provides consistent sky imagery, diffuse illumination and roughness-dependent reflections, with interactive asset replacement for acceptance.

## What Changes

- Extend native textures, shader reflection, material binding and D3D12 uploads with linear floating-point 2D and cube textures.
- Import equirectangular Radiance HDR and EXR through tracked asset IO; bake radiance cubemaps, irradiance SH and GGX prefiltered cubemaps offline. Share a BRDF integration LUT.
- Add a native sky asset and a mutually exclusive constant/sky environment source on scene environment lights, retaining references and edits through serialization and asynchronous replacement.
- Draw the sky after opaque geometry and before transparency in the shared HDR pipeline; evaluate common IBL in Forward and Deferred, including clustered variants.
- Add SceneViewer native sky asset selection, path application, loading/error status, visibility, intensity and rotation controls.
- Include three CC0 Poly Haven environments with provenance, migrate Sponza and validate numerical, GPU, lifecycle and persistence behavior.
- Keep implementation uncommitted through the quality audit, then archive and commit after audit closure as requested on 2026-09-13. Dynamic probe capture, spatial visibility/GI and physical sun extraction are outside this change.

## Capabilities

### New Capabilities
- `skybox-skylighting`: Native HDR sky import, shared environment preprocessing, infinite sky rendering and global diffuse/specular IBL.

### Modified Capabilities
- `texture-assets`: Floating-point and cube native texture storage and upload.
- `scene-lights`: Asset-backed environment source, independent background visibility and atomic publication.
- `scene-viewer`: Native sky selection and persistent environment editing.
- `material-parameter-binding`: Typed cube texture reflection and resource binding.

## Impact

Textures, Assets, AssetImport, Scene, Materials, Shaders, RHI, Renderer, D3D12, AssetTool and SceneViewer; a focused CPU Environment module; native content build and test fixtures. Existing texture and environment records migrate to their previous 2D RGBA8 and constant-color behavior. Existing stb_image/TinyEXR adapters are extended without exposing third-party APIs.
