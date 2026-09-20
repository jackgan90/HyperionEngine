## Context

Editor has Main-owned single selection and a retained RGBA8 viewport rendered by FSceneRenderPipeline. SceneBridge owns object-to-render bindings. IRenderFeature already runs AfterTonemap before GUI composition. RHI supports sampled R8 color targets and Maximum blending; material rendering currently accepts single-sample targets only.

## Goals / Non-Goals

**Goals:** orange exterior silhouette outlines through scene occluders; union mode by default and independent per-object mode selectable per viewport; multi-target input from the first implementation; reusable Renderer logic and safe owned frame data; real image verification and comparison outputs.

**Non-Goals:** multi-selection input/transactions, object-ID picking, scene serialization of selection, outlining non-geometric light/camera icons, recursive selection of child objects, dynamic plugin selection, arbitrary material deformation inference, native MSAA support, commit/push.

## Decisions

1. Editor alone appends MakeSelectionOutlineFeature at startup, like transient geometry. Renderer exposes immutable FSelectionOutlineRequest and a setter on the pipeline/context. No Scene/RHI dependency or Editor dependency is introduced into Runtime algorithms.
2. Main resolves each selected scene handle through SceneBridge into its own primitive group, tagged with the exact FScenePublicationToken. Frame freezing follows scene publication. Render rejects mismatched publication and ignores expired generation handles. Collection fetches only requested primitive handles, before batching, and preserves current geometry, world transforms, section visibility and material inputs. Diagnostic snapshots are not rendering APIs.
3. Union mode clears a single R8 object mask, rasterizes all groups without depth testing/writes, and extracts exterior edges once. Per-object mode clears/rasterizes/extracts each group, accumulating edges with Maximum blending. All sections of one object share a mask. Both modes ignore scene depth. O = max(0, Dilate(M, radius) - M); real holes remain silhouette boundaries. Neither normals nor ordinary depth discontinuities participate.
4. Reuse a mask and an accumulated outline target across objects/frames; clear every active frame and between groups. Empty requests add no passes and release feature targets. Resize/reset retires sources through existing retention scopes/fences. Request mode and all deferred inputs are owned per build.
5. Composite once at AfterTonemap using alpha blending into the loaded output's sRGB view. Read only the outline texture, not the output attachment. Preserve destination alpha. GUI/gizmos remain above it. Default width is two physical viewport pixels with an optional 2x mask supersampling quality setting; baseline is single-sample, not implicit MSAA.
6. Standard Model.hlsl materials receive a cached runtime auxiliary mask definition preserving their vertex program, instance layout, semantic declarations and effective alpha inputs. Add a dedicated shader permutation to output coverage while reusing ClipModelAlpha. Explicit custom SilhouetteMask passes are accepted; unknown shader families produce a controlled unsupported diagnostic instead of an inaccurate fallback. No persistent material is edited and existing native PBR assets need no migration. Blended surfaces use geometric coverage; masked surfaces use existing cutoff.
7. Editor exposes Union / Per object in Viewport options, applied next frame without dirtying the document. A dedicated comparison exercise supplies multiple scene handles without changing normal single-selection UI, writes images for both modes, and checks switching/selection/lifecycle behavior.

## Risks / Trade-offs

- Per-object work scales with selected count -> union default, direct subset collection, fixed texture count, shared geometry/material preparation, and test pass-count differences.
- Auxiliary material parameters can lose instance overrides -> preserve snapshots and effective semantic/name overrides; validate opaque/masked, multi-section and mirrored geometry on GPU.
- New frame inputs can outlive selection/document -> publication/epoch/generation validation and immutable captured requests.
- Single-sample raster aliasing -> small exterior kernel plus optional supersampled mask, verified visually; do not add backend MSAA just for outlines.
- Unsupported custom material -> explicit mask usage contract and bounded diagnostic, no false success or unbounded cache.

## Migration Plan

No serialized data changes. Add the Renderer feature and editor wiring, run targeted regressions and document the display options. Removing Editor activation disables all outline allocation without changing normal scene rendering. Leave the completed change unarchived and uncommitted for user review unless subsequently instructed otherwise.

## Open Questions

None blocking. Transparent coverage follows geometric silhouette; real holes count as silhouette boundaries. Performance tuning beyond direct subset collection and union batching follows measurements.
