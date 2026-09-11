## Context

Perspective uses RH zero-to-one standard depth. D32 targets clear to one, materials use LessEqual, and transparent items sort projected depth descending. CSM has its own orthographic projection, comparison sampler and receiver/caster bias. Triangle bypasses camera projection and currently disables depth. Viewer snapshots settings before Render/RHI work and already supports immutable target lifetimes.

## Goals / Non-Goals

**Goals:** Default reversed-Z with a startup-only standard-Z fallback across all three applications, both scene pipelines and CSM. Preserve material escape hatches, clipping, transparency, ordinary/instanced parity and disabled-depth UI behavior.

**Non-Goals:** Runtime mode switching, file watching, infinite far planes, new graphics backends, unrelated rendering optimizations.

## Decisions

1. Math owns EDepthConvention and depth mapping helpers without RHI dependencies. Keep the existing four-argument Perspective standard for source compatibility; applications explicitly select their startup convention. Renderer views and standalone fullscreen pass descriptions carry an explicit convention. Physical Near/Far remain positive and ordered.
2. Add a view-relative material depth policy, defaulting to raw comparison for existing external/custom materials. Built-in model and Triangle passes opt in. Resolve relational comparisons and depth biases before both PSO lookup and creation; stencil comparisons are unaffected. This avoids silently changing explicit Less/Greater semantics. The renderer consumes per-view convention, not mutable global state.
3. Application settings default reversed_z=true and expose a restart label. Viewer captures the active mode after loading settings and copies it into submitted frame settings, while editable settings retain the desired next-launch value for saving. No in-flight frame observes a partial edit.
4. Clear sampled scene depth and frame depth to the convention far value. Resource optimized clear metadata agrees with pass clears. Keep viewport depth range ascending. Existing finite-frustum clip inequalities and inverse-matrix Deferred reconstruction remain applicable.
5. Main, shadow and legacy views propagate convention explicitly. CSM changes projection, clear value, neutral depth, sampler comparison and receiver/caster bias direction together. Normal offsets retain world-space meaning. CSM preparation cache includes convention and texture recreation checks clear value as well as resolution.
6. Normalize transparent projected sort keys to standard ordering, including clip-space items. Triangle enables depth test/write; Renderer maps canonical clip depth to the active convention while preserving its geometric World and screen-space shape.
7. Include convention in preparation environment comparisons. PSO state equality already covers resolved compare/bias, but all ordinary/instanced lookup paths must supply the same convention. Startup mode is fixed at the application boundary; low-level per-view data stays explicit.

## Risks / Trade-offs

- Mixed raw/view-relative materials -> document the opt-in policy and test both modes, including explicit raw compares.
- Shadow bias inversion errors -> verify lit/occluded GPU fixtures, masked casters and both rendering paths.
- Cache reuse hides state changes -> cover differing view conventions and resolved PSO identity while preserving resource ownership.
- Existing tests assume standard projection -> retain legacy low-level defaults and add explicit reversed coverage rather than weakening assertions.
- Startup configuration can appear live in reflected UI -> show desired restart value separately from active mode and test frame snapshot behavior.

## Migration Plan

Existing application configurations without the new field adopt reversed-Z. Set properties.reversed_z=false and restart to restore standard-Z. Built-in materials follow views; custom materials retain raw comparison until explicitly opting in. No serialized IDs are renamed. Validate Debug/Release and record results in implementation.md; archive/commit only when requested separately.

## Open Questions

None blocking implementation. Exact helper placement and test fixture reuse will follow existing module boundaries.

## Audit refinement: clip-space geometry

For `bClipSpace` primitives, World remains a geometric transform into canonical Standard clip depth. Renderer applies the selected depth mapping when producing WVP, culling and sorting. Triangle no longer stores a depth projection in World: doing so changed its determinant without changing screen winding, corrupting face selection. Custom shaders bypassing the engine WVP semantic remain responsible for raw output conventions. View material input identities also include the depth convention, so unchanged matrices cannot retain stale clip-space WVP values.
