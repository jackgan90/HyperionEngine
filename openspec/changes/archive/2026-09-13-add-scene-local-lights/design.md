## Context

Scene owns typed nodes and reflected persistence. SceneBridge publishes immutable metadata under exact tokens. Deferred has four GBuffer attachments, sampled D32, HDR accumulation and a shared BRDF; transparent materials remain forward. Current RenderGraph rejects simultaneous depth attachment/SRV use. RHI already exposes front culling, disabled depth clipping and additive blend state.

## Goals / Non-Goals

Implement persistent point/spot lights, existing visibility integration, traditional Deferred volume rendering, editable diagnostics, and a visually tuned Sponza example. Do not implement clustered/forward local lighting, local shadows, area lights, glTF punctual import, IBL/GI or commit changes.

## Decisions

1. Add FScenePointLight/FSceneSpotLight payloads and node kinds with Color, Intensity, Range and spot inner/outer half angles. World position and normalized minus-Z emission derive from hierarchy. Range is in world units and does not inherit scale. Validate finite nonnegative radiance, positive finite range and 0 <= inner < outer < pi/2. Preserve immutable node kind.
2. Extend source v2 to v3, native scene v4 to v5 and node record v1 to v2 with explicit migrations; old scenes have no added local lights. Bump source/native importer revisions. Preserve old IDs, selections and empty scenes.
3. Publish light values with scene metadata. Maintain a Render-owned light index using the existing ISceneSpatialIndex implementation and ISceneVisibility query contract, separate from model/caster membership. Cache unchanged bounds; rebuild/refit only when light membership/bounds change. Query each view with its culling matrix and None/Linear/Bvh mode. Per-view lists own values, never pointers into editable Scene.
4. Extract direct BRDF from the common lighting function without changing directional output. Local radiance uses saturate(1-(d/R)^4)^2 / max(d*d, .01*.01); at the exact light center use a finite neutral direction. Intensity is the inverse-square numerator in current world units. Spot weights use smoothstep between outer/inner cosine. Ambient, emissive, CSM and shadow debug remain outside local direct evaluation.
5. Add a dedicated volume raster pass after fullscreen lighting, before compatibility/transparency. Share reusable outward-facing closed conservative sphere/cone meshes. Use CullFront for both camera locations, disabled Z clipping, no depth attachment/test/write, RGB One+One and unchanged alpha. Reconstruct visible receivers from sampled SceneDepth and reject invalid GBuffer/range/cone pixels. Circumscribed proxy geometry prevents tessellation leaks. The convex exit surface covers each receiver once; no fullscreen production fallback and no light-list shader loop.
6. Render declares immutable pass inputs; RHI 0 prepares reflected material bindings and indexed draws through existing resource caches and graph leases. Reuse geometry/programs/PSOs and scope-owned bindings. The scene snapshot is reusable by a future clustered consumer; the volume renderer is pipeline-specific.
7. Forward and Deferred transparency receive no local contribution. Report algorithm availability and total/candidate/visible/drawn light counts. SceneViewer creates/edits both types and displays their influence bounds.
8. Tune authored Sponza point positions/ranges/intensities against the README Screenshot (https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Sponza/README.md; image screenshot/large.jpg). Preserve the existing calibrated camera and original model/materials. Target distinct soft floor pools and nearby column/banner illumination, not pixel identity or unimplemented local shadows. Capture local-on/off and reference comparisons on real D3D12.

## Risks / Trade-offs

- No hardware early depth rejection increases shaded proxy pixels; conservative frustum culling, finite volumes and early receiver rejection bound work. Measure with existing profiling.
- Clip-plane crossing or incorrect winding can omit/double contributions; test outside/inside/boundary, near/far clipping, both depth conventions and sub-viewports with analytic lighting expectations.
- Large radiance and near-center values can overflow HDR; reject nonfinite authored/derived inputs and ensure shader output remains representable.
- Current CSM debug output must not be contaminated by additive local light; suppress local accumulation for active shadow visualization modes.
- Reference has unpublished lighting/postprocessing and local shadows; record visual differences without changing global BRDF or adding unrelated rendering features.

## Migration Plan

Add version migrations, update import cache revisions and regenerate native sample content with normal builds. Existing source/native scenes remain readable. Save new data in the new versions; old binaries reject new versions explicitly. No dependency changes or Git commit.

## Open Questions

No blocking product decisions. Resolve numeric proxy tessellation and Sponza light parameters through targeted GPU validation and visual comparison.
