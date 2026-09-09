## Context

The current material system already has reflected bindings, pass Usage, immutable frame scopes, view-local evaluation and instancing. RenderGraph and swapchain recording still assume one color target and one internal depth buffer. SceneViewer has a ground slab but no shadows. The user approved implementation after proposal generation, requested shared shader source/permutation macros and excluded staggered updates and Git commits.

## Goals / Non-Goals

**Goals:** true offscreen depth-only rendering; explicit resource dependencies; renderer-owned forward orchestration; one scene with a main view and four shadow views; stable single-light CSM; opaque/masked casting; measurable CPU/GPU cost and regression coverage.

**Non-Goals:** staggered updates, multiple shadowed lights, transparent colored shadows, PCSS/VSM, GPU-driven culling, transient resource aliasing, deferred rendering, Git commit or automatic archive.

## Decisions

### Pipeline and view family

FForwardRenderPipeline orchestrates shadow preparation, main forward passes and extension stages. FCascadedShadowMap owns stable view identities, cascade configuration and persistent texture source descriptions. Existing BuildViews remains a compatibility entry point; the family path shares frozen inputs and refreshes spatial data once before view-local collection. Per-view visibility/draw/batch counters remain separate from family totals. Main depth and shadow attachments have explicit independent identities. Plugins receive the existing owned frame data and append extension work at the pipeline-defined stage.

### Attachments and barriers

Graphics passes identify whether they use the imported swapchain color target, an explicit depth texture, and sampled depth textures. Graph compilation validates initialization/read/write hazards and emits per-resource pre/post transitions. Zero color attachments are valid only with compatible pipeline state/output signatures. Existing swapchain-depth calls remain supported for direct clients. Native recording retains attachments and transition resources in addition to draws, including clear-only passes. All work uses the existing graphics queue, parallel CPU recorders and ordered submission. Stable initial/final ShaderRead states keep cross-frame and cancelled-unsubmitted graph state deterministic. Initialization is queued, not implemented with per-frame idle waits.

### Rendered texture binding

Extend the engine-owned CPU texture source description to represent a sampled depth target with immutable size/identity and initial clear depth. The existing device/session material texture cache resolves that source to an RHI texture and also exposes the same texture for graph attachment use. Materials never carry native objects. This avoids a second hard-coded shadow binding path and permits custom shaders to sample rendered depth. Source replacement changes resource identity; rendering new depth into the same texture does not recreate descriptor tables. A shared initialized 1x1 depth source provides the disabled-shadow fallback. Comparison sampler descriptions include the compare operation and are checked against reflected sampler kind and device capabilities.

### CSM mathematics and caster selection

Use four independent D32 2D maps, 2048 square by default, mixed uniform/logarithmic splits (lambda 0.6), explicit finite shadow distance (100 units initially), and all active cascades updated every frame. Camera view/projection and near/far are explicit; matrices are right-handed, column-major, depth zero to one. Fit each receiver interval with a stable bounding sphere, pad XY for filtering/snapping and snap its center in a stable light basis to world-texel increments. Choose a non-collinear basis with continuity for changing lights. Degenerate/nonfinite configuration is rejected or disables shadows with finite fallback values; valid vertical/horizontal and dueling-frusta light directions remain supported.

Query casters independently from main-camera visibility. A receiver extrusion toward the light intersects the shared spatial index, includes offscreen occluders and supplies caster-aware conservative Z bounds. Unknown bounds fail open. Shadow distance limits receivers, not arbitrarily the caster list. Stabilize conservative Z extents without clipping relevant casters. Empty cascades still clear their depth target.

### Shader permutations and artifacts

Model.hlsl uses HYP_SHADOW_CASTER with the existing HYP_ENABLE_INSTANCE. Shared world-position and alpha routines keep transforms, vertex alpha, UV selection, base-color factors and cutoff identical. Opaque ShadowDepth omits PS; masked ShadowDepth runs only alpha clipping and outputs no color. Blend receives shadows but does not cast. Missing ShadowDepth usage is skipped before material preparation. Both caster variants retain correct double-sided/mirrored culling and batching. Cascades share shaders and vary only constants/targets.

Forward multiplies only directional direct lighting by shadow visibility. View/pass shadow data contains light matrices, split distances, texel sizes, bounded receiver/normal bias and fade widths. Fixed small PCF uses comparison sampling at level zero; cascade overlaps blend two samples only in the transition band. Tight conservative depth ranges, clamped slope raster bias, geometric-normal offsets and bounded receiver-plane gradients balance acne and detachment. UV/depth boundaries and invalid projections return unoccluded visibility. Stable projections, finite filters and final-distance fading address temporal shimmer, border sampling and cascade seams.

### Performance and observability

Persistent 4x2048 D32 payload is approximately 64 MiB (4x1024 is 16 MiB), plus alignment/descriptors. No per-frame depth allocations, shader/PSO creation, readbacks or queue-idle synchronization. View constants are shared; unchanged geometry/material/instance data is cached. Expose named per-pass GPU timestamps and CPU preparation/caster/draw/upload counters with delayed fence-safe query readback. Preserve the backend's 16-context validation and standard pipeline headroom.

Warm static/moving camera and moving-light off/on runs use the same executable, serialized GPU work and fixed configuration. Record mean/P95, CPU scopes, shadow depth GPU sum, forward GPU increment, allocation counts and longer-run stability. Initial Showcase engineering targets at 4x2048 Release are shadow depth mean <=1ms/P95 <=2ms and added CPU preparation mean <=1ms; report actual device/configuration and any deviation rather than claim unmeasured guarantees.

## Risks / Trade-offs

- Bias trades acne for detachment -> calibrated contact/sloped/thin/masked fixtures, clamp and scale-aware controls.
- Extreme light directions increase real caster overlap -> independent conservative culling, shared geometry/batches and explicit per-cascade cost; never hide missing casters as optimization.
- More views stress CPU caches -> stable identities, active-pass-only parameter work and bounded caches; observe unchanged uploads and descriptors.
- Offscreen resources outlive failed frames -> retain all command references through existing fence/cancel/failed-Present protocols and add regressions.
- Fixed resolution cannot represent arbitrarily fine geometry -> document settings and demonstrate stability over the supported fixture range.

## Migration Plan

Add foundation contracts and tests, integrate pipeline and views, implement shaders/CSM, then enable SceneViewer shadows. Existing direct RenderSession clients default to shadow-disabled shader inputs. Existing CLI/config keys and plugin IDs stay valid; new controls are additive. Validate with Debug/Release builds, targeted and full regressions, image/sequence checks and repeated performance runs. Leave the completed change and code uncommitted for user review.

## Open Questions

No blocking product questions. Bias defaults and performance targets will be calibrated on the actual scene/device and documented with evidence.
