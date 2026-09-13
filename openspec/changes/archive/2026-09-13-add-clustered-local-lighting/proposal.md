## Why

Scene point and spot lights currently affect only Deferred opaque/masked geometry through individual volumes. A shared clustered lookup will make local lighting available to Forward models and transparency while consolidating Deferred lighting work.

## What Changes

- Build per-view three-dimensional light clusters on CPU, publishing immutable headers, indices and light attributes through existing structured read buffers.
- Share exact point/spot evaluation across clustered Forward, clustered Deferred and the retained volume path.
- Fuse clustered local and directional lighting in Deferred when a main directional light contributes; otherwise use a dedicated fullscreen cluster/environment/emissive pass.
- Enable clusters by default with persisted settings, CLI and GUI controls. Disabling restores existing Deferred volumes and excludes local lights from Forward/transparency.
- Preserve existing SceneViewer Sponza point-light appearance, Scene data and local shadow exclusions, with GPU captures and bounded-resource validation.

## Capabilities

### New Capabilities
- `clustered-local-lighting`: Per-view conservative cluster construction, buffer contracts, cross-pipeline lookup, caching and diagnostics.

### Modified Capabilities
- `scene-local-lights`: Default clustered routing with opt-out to the current Deferred-only light volumes, including updated Viewer availability and acceptance.

## Impact

Renderer pipeline and material binding, Materials semantics and built-in PBR descriptions, shader includes, Config/Viewer controls, asset importer cache revisions, tests and documentation. No Scene serialization change, compute/UAV infrastructure, local shadows, new light types, Git commit or automatic archive is included.
