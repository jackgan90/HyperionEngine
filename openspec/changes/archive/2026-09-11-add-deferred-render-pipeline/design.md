## Context

At baseline 2a1ad62, FForwardRenderPipeline orchestrates CSM and the main view through FRenderSession, and Viewer adds display extensions. RenderGraph and D3D12 accept only one backbuffer color attachment and sampled D32 offscreen depth. Model.hlsl combines material sampling, lighting and Reinhard tonemapping; ModelPreparation already treats base/emissive textures as sRGB and generates their mips in linear space. DepthPreview already owns an oversized fullscreen triangle. Retained snapshots, draw packets and native plans are important existing CPU optimizations.

## Goals / Non-Goals

**Goals:** default traditional raster Deferred; shared Forward/Deferred linear lighting and output; validated configurable GBuffer; preserved CSM, material modes, instancing, ownership and asynchronous frame preparation; reusable fullscreen passes; measured performance and image comparisons.

**Non-Goals:** compute/tiled/clustered lighting, mobile single-pass deferred, MSAA, new lights or BRDF models, multi-model stencil dispatch, transient resource aliasing, unrelated renderer optimization.

## Decisions

### Explicit target contracts

Add engine-owned color formats, a sampled render-color texture description and capabilities. Color attachment lists and complete target signatures propagate through RenderPass, RenderGraph, FPassCommands, FGraphicsTarget and D3D12 RTV/PSO recording. Preserve the established single-target API where practical while giving MRT one canonical ordered interpretation. Every attachment independently participates in load/store, dimensions, hazards, content validation and lifetime retention. All prepared and native caches compare complete target signatures; hashes are bucket selectors only.

Textures used by the pipeline are immutable source descriptions resolved on RHI 0 through existing resource ownership. Reallocate on size/layout changes and retain old descriptions/resources through their existing submitted users. Export reusable scene targets in ShaderRead state; transition scene depth back to DepthWrite before forward compatibility/transparent work. First version does not bind depth simultaneously as SRV and DSV.

### Pipeline ownership and routing

Renderer owns Deferred and shared scene-pipeline operations. CSM views remain independently culled; main geometry preparation reuses existing view/cache/batch machinery. DefaultLit opaque/masked materials expose a DeferredBase usage; their forward equivalents expose HDR shading usages, with no duplicate contribution. Unsupported/depth-compatible material modes use explicit forward compatibility routes and diagnostics. Transparent draws remain globally depth sorted. Unlit is supported without a Deferred shading-model ID. Existing custom direct-backbuffer materials retain their legacy contract where they cannot participate in HDR scene shading, and this distinction is surfaced rather than silently assuming their output is HDR.

Frame order: shadow maps, Deferred BasePass, fullscreen lighting to SceneColor, forward opaque compatibility, forward transparent, fullscreen tonemapping, display extensions/GUI, Present. Forward selection substitutes HDR geometry shading for BasePass/lighting and shares the remaining stages. Stable main-view identity and statistics must not be replaced by shadow or compatibility-view statistics. Frozen frame jobs capture their own resources/settings, never mutable pipeline state.

### Configurable GBuffer

FGBufferLayout describes ordered formats, fixed semantic packing, clear values and a validated layout signature. Initial standard layout: RGBA8 base/metallic; RGBA16F oct-encoded shading and pre-normal-map normals; RGBA8 roughness/AO/validity/reserved; RGBA16F emissive/reserved. A higher precision preset upgrades normalized color/parameter storage to RGBA16F. Format overrides compatible with the supported packing are allowed and validated; arbitrary shader packing requires a new layout version. Neither shader register layout nor PSO formats are scattered constants.

World position reconstructs from sampled D32 using inverse view-projection, viewport origin/extent and the engine's 0..1 depth convention. GBuffer reads use exact pixel loads. Invalid pixels produce the configured background without evaluating undefined normals. Initial buffers use linear/data encodings, not sRGB GBuffer storage. Packing and decoding live in one shader include. Runtime shading-model selection is a future boundary, distinct from material identity.

### Shared shaders and color contract

Split model vertex/material initialization, alpha coverage, material parameter structs, BRDF/lighting, shadows, GBuffer encode/decode and tonemapping into focused includes/entrypoints. Macros choose Forward HDR, Deferred Base and ShadowDepth; existing instance permutations remain supported. BasePass does not evaluate lighting or retain unnecessary scene-light resource bindings. Both lit paths call the same lighting function and output unbounded linear HDR.

BaseColor/emissive texture RGB decode sRGB; alpha and numerical maps stay linear. Material factors, lights and vertex scene colors are linear. SceneColor is RGBA16F. Reinhard with exposure 1 remains the initial common tone operator; the final pass writes linear display values to an sRGB RTV exactly once. Explicit output settings distinguish authoring/display clear color from HDR scene light. GUI runs after tonemapping and decodes authored sRGB vertex colors before blending through an sRGB target; font coverage remains data. Debug images are display overlays. Unlit and transparent outputs use the same final transform, an intentional change from legacy per-material tonemapping.

### Shadow receiver continuity

Current CSM uses the normal before normal mapping and ddx/ddy of interpolated world position. Preserve both normals and expose receiver gradients explicitly to shared lighting. Deferred reconstruction must reject cross-surface derivative samples at silhouettes; use continuous depth neighbors with bounded/fallback receiver correction. Validate shadow edges, thin geometry, grazing angles, cascade transitions, mirrored/double-sided surfaces and camera motion. Quantization and receiver reconstruction allow small documented numeric differences; shared formulas do not promise bitwise parity.

### Fullscreen pass reuse

Generalize DepthPreview's existing oversized indexed triangle; do not add a second quad implementation or require non-indexed draw support. Renderer fullscreen descriptors declare shaders, reflected bindings/constants, outputs, reads, viewport/scissor, state and stable pass names. RHI 0 creates/caches resources, while Render declares graph access. Lighting, tonemapping and depth previews use this facility; it bypasses primitive collection.

### Validation and performance

Capture the baseline executable/build identity before replacing builds where available. Compare legacy Forward, common-HDR Forward and Deferred, keeping workload and CSM/instancing/output parameters fixed. Report static/moving SceneViewer and ModelViewer, multiple resolutions/layouts, repeated alternating runs, CPU median/P95, pass GPU intervals, draw/PSO/binding/upload counts, logical/actual texture allocation, timing drops and validation/readiness. Use existing submission-aligned GPU timing and profiling tools; never add waits to obtain timings. Separate Debug correctness and Release performance. Include HDR/final-image comparison, color ramps, depth/normal visualization, lifecycle/resize and negative MRT contracts. Initial single-directional-light workloads may be slower on Deferred; report regressions honestly.

## Risks / Trade-offs

- GBuffer bandwidth and pass overhead can dominate one-light scenes -> measure both pipelines and precision layouts; do not promise a speedup.
- Silhouette receiver gradients can cause shadow mismatch -> explicit continuity handling and targeted images, not blind fullscreen ddx.
- API expansion can bypass established validation/cache checks -> negative tests for formats/count/dimensions/aliasing, retained targets and shader outputs.
- Additional view usages can duplicate preparation/draws -> retain cache boundaries and verify exclusive coverage and stable main-view statistics.
- Output migration changes transparent and Unlit pixels -> update expectations only with explicit linear-HDR evidence, retain common Forward as reference.
- Future stencil dispatch needs read-only depth/aspect state -> keep aspects separate and document unsupported simultaneous use rather than introducing premature native special cases.

## Migration Plan

Implement format/MRT foundation first, then fullscreen/shared HDR Forward, then Deferred and configurable selection, then diagnostics/tests/performance. Existing CLI/config keys and target names stay stable; new renderer settings default to Deferred. Explicit Forward is the operational fallback. Do not archive or commit without a subsequent request; deliver the completed change and evidence for review.

## Open Questions

No user decisions block implementation. Receiver-gradient tolerance and performance sample sizes are resolved through measured validation and recorded in implementation evidence.
