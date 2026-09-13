## Why

Scene currently supports directional and environment lights but cannot represent or render finite point and spot lights. Deferred light volumes will add traditional multiple-light rendering and allow the default Sponza scene to approximate the localized illumination in the Khronos reference screenshot.

## What Changes

- Add distinct scene-owned point and spot light types, hierarchy-aware transforms and enablement, runtime editing, versioned source/native serialization and migration.
- Publish immutable local-light data with existing scene publication tokens; reuse the visibility/spatial-index contracts for per-view conservative light culling.
- Share the existing direct-light BRDF across directional, point and spot lighting; add smooth finite inverse-square and cone attenuation.
- Accumulate one draw per visible local light through closed conservative sphere/cone geometry in Deferred. Use CullFront inside and outside, no depth attachment/test/write, disabled Z clipping, and exact reconstructed receiver tests.
- Keep this algorithm inactive in Forward, including Deferred's forward transparent route; expose that limitation in diagnostics. Clustered lighting, local shadows, RectLight, IBL/GI and glTF light import are outside scope.
- Add editable point lights to the current Sponza scene and visually approach the floor light pools and nearby architectural illumination in the user-selected screenshot; deliver real GPU captures and validation evidence.

## Capabilities

### New Capabilities

- `scene-local-lights`: Typed persistent point/spot lights, coherent publication, conservative culling, Deferred volume accumulation, pipeline eligibility, diagnostics and Sponza acceptance.

### Modified Capabilities

None. Existing directional/environment lighting and scene publication guarantees remain intact; the new capability extends them additively.

## Impact

Scene, AssetImport, Renderer, shared/deferred shaders, SceneViewer, Sponza scene content, tests and documentation. Reuse engine-owned RHI/material/resource contracts without new vendor dependencies. No Git commit is requested.
