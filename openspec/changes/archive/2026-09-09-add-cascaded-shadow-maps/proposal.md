## Why

The forward renderer cannot currently render and sample an offscreen depth attachment, and frame pass ordering lives in the Viewer. Directional cascaded shadows need an explicit renderer pipeline, shared-scene multi-view preparation, stable shadow projections and measurable bounded cost.

## What Changes

- Introduce a Renderer-owned forward pipeline that schedules shadow depth passes, forward scene passes, extension passes and presentation.
- Extend graphics attachments, resource transitions and retention to sampled D32 depth textures and depth-only pipelines; enable validated comparison samplers.
- Prepare one frozen scene for the main view and up to four independently culled shadow views, refreshing spatial data once and keeping per-view statistics.
- Add stable directional CSM with finite caster-aware bounds, texel snapping, bounded bias, PCF, cascade blending and distance fading. All active cascades update every frame; staggered updates are excluded.
- Generate forward and shadow-caster permutations from the same Model.hlsl source using macros. Opaque casting omits PS; masked casting shares alpha evaluation; both support instancing.
- Bind renderer-produced textures through engine-owned resource references without CPU readback or per-frame resource/descriptor recreation.
- Enable shadow presentation and controls in SceneViewer, add artifact regressions, GPU/CPU timings and repeated performance evidence.

## Capabilities

### New Capabilities
- `cascaded-shadow-maps`: Stable single-directional-light CSM, caster selection, shared shader permutations and bounded performance validation.
- `forward-render-pipeline`: Independent pass orchestration and scene view-family preparation.

### Modified Capabilities
- `render-graph-plugins`: Explicit offscreen depth attachments and resource read/write dependencies with retained resources.
- `graphics-resource-binding`: Sampled depth textures, comparison samplers and renderer-produced resource binding.
- `scene-viewer`: Shadow-enabled demonstration, controls and diagnostics.

## Impact

Math, Materials, Renderer, RHI, D3D12, builtin HLSL, Viewer/SceneViewer and tests change. Scene and Materials remain independent of RHI; native details remain in D3D12. Existing executable names, plugin IDs, serialized keys and non-shadow workflows remain supported. No external dependency, network exposure, staggered cascade updates or Git commit is included.
