# Scene local lights

Point and spot lights are scene-owned nodes with independent types. They support normal hierarchy, enablement, stable IDs, generation handles, runtime editing and source/native save and load. [Clustered lighting](ClusteredLighting.md) is enabled by default for Deferred, HDR Forward and lit transparency. Disabling it restores traditional Deferred opaque/masked volumes and excludes Forward/transparent local lighting. Unlit and legacy display materials receive no local contribution. Local shadows, area lights and glTF `KHR_lights_punctual` import are outside this implementation.

## Authoring

Source scenes use `schema_version: 3`. A node accepts one `pointLight` or `spotLight` payload, mutually exclusive with every other payload:

```json
{"id":"lamp","translation":[0,1,0],"pointLight":{"color":[1,0.94,0.8],"intensity":8,"range":4}}
```

```json
{"id":"spot","translation":[0,3,0],"spotLight":{"color":[1,1,1],"intensity":10,"range":5,"innerRadians":0.35,"outerRadians":0.6}}
```

Color is linear RGB. Intensity is the inverse-square numerator in the scene's world units, not an imported lumen value. Range is the finite radial cutoff in world units; node scale does not scale it. A point uses world translation. A spot uses the same orthonormal pose extraction as the scene camera, emits along world-transformed local minus-Z, and uses inner/outer **half angles**. Its support is the intersection of the range sphere and angular cone. Accepted angles satisfy `0 <= inner < outer < pi/2` with distinct representable cosines. Color, intensity, radiance, range and derived cone size must be finite and representable.

Distance attenuation is `saturate(1 - (distance/range)^4)^2 / max(distance^2, 0.0001)`. The denominator uses a 0.01-world-unit near-source distance. Spot attenuation multiplies by smoothstep from outer cosine to inner cosine. A finite neutral direction handles the exact light center. Pathological per-light shader values are bounded before HDR output; RGBA16F additive accumulation still has finite dynamic range, as does existing scene lighting.

Native scene schema is v5 and node records are v2. Old source v1/v2 and native v1-v4 continue to migrate; old empty scenes remain empty and no local lights are synthesized. New local payloads require source v3. Both scene importers now use revision 3, invalidating stale conversion results. Runtime snapshot/save uses the same Scene reflection records.

## Publication and visibility

SceneBridge publishes point/spot values with camera and geometry under the existing exact publication token. A separate local-light revision changes only for local-light structure, transform, enablement or light-parameter edits. Camera-only publications reuse the derived light records and spatial index. Render never reads mutable Main scene state.

The light index reuses `ISceneSpatialIndex` / `CreateBvhSpatialIndex()` and accepts `ISceneVisibility`. The scene pipeline supplies its current or frozen culling frustum; None, Linear and BVH modes remain supported. Range spheres and finite spot support have conservative AABBs. Light-center visibility alone is insufficient. Candidate IDs are sorted before accumulation so BVH topology does not change light summation order. Light membership and diagnostics are independent of model geometry and CSM casters.

## Legacy Deferred volume rendering

With clustering disabled, the graph executes BasePass, directional/environment/emissive fullscreen lighting, local light volumes, compatibility/transparent geometry and tonemapping. DirectLighting.hlsli contains the common BRDF and LocalLighting.hlsli shares single-light attenuation with the clustered path; ambient, emissive, CSM and shadow visualization stay outside local-light evaluation. Local accumulation is suppressed during shadow visualization.

Each visible light submits one indexed draw into the existing linear HDR scene color. The session caches a closed sphere (512 triangles, conservatively expanded from a subdivided octahedron) and closed cone (48 sides with a bottom cap, 96 triangles). Circumscribed proxies cover analytic influence rather than cutting off tessellation edges.

Both inside and outside cameras use `CullFront`: an intersecting view ray has one convex exit surface. Depth test/write are disabled and SceneDepth is sampled without a DSV. Z clipping is disabled so a far-clipped exit does not create a hole; ordinary positive-W and screen clipping remain. The pixel shader reconstructs the actual receiver, rejects invalid GBuffer/range/cone pixels, and adds direct RGB once while preserving alpha. No fullscreen production fallback, per-pixel light list loop, stencil format or read-only DSV extension is introduced.

Render declares owned immutable pass descriptions; RHI 0 prepares materials and draws using the existing reflection, binding, constant, PSO and graph lifetime machinery. Older queued passes keep their textures, parameters and GPU owners when lights or target generations change. Numeric constants use a tracked pass lifetime separate from the target/PSO lifetime: retiring even a stationary pass schedules the existing constant-cache collection. Recorded buffer slices retain pages until normal GPU retirement. Geometry and PSOs are reused, and GPU retirement uses existing frame fences.

## Viewer and measurement

The SceneViewer panel creates/edits both light types, shows point/spot visible and total counts, candidate and draw counts, and reports when the algorithm is inactive. `Show light influence` displays point range circles and spot inner/outer boundaries. All edits persist through the existing Save button or `--save-scene`.

Viewer benchmark CSV appends `local_lights_active`, `point_lights`, `spot_lights`, `local_visible`, `local_draws`, `local_query_ms`, `local_rebuilds`, `local_refits` and `local_gpu_ms`. Existing columns and model counts retain their meaning. The existing completed-submission GPU timing supplies the local pass time; existing category-gated profiling scopes cover index updates, culling and RHI preparation.

## Sponza reference

The default `assets/Scenes/Sponza.json` contains three warm point lights arranged along the courtyard. The camera, source model, textures, material values, directional light and exposure are preserved. Light positions were chosen from the calibrated camera's ground-plane projection of the reference floor pools, then tuned using real D3D12 captures for nearby column/banner illumination.

Reference: [Khronos Sponza README Screenshot](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Sponza/README.md), image `screenshot/large.jpg`. Upstream explicitly states that its lights are not part of the model. This is an authored visual approximation: reference light parameters are unpublished, and local shadows, GI/IBL, sky and tonemapping differences are not reconstructed.

Validation evidence, screenshots and measurements are recorded in the change's `verification.md` and `out/LocalLights`.
