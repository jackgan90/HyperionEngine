## Context

`FRenderView::Usage` is an open material-pass selector. Four production sites also interpret `ShadowDepth` as transient/statistics behavior: SessionViewCache, TransientGeometry, RenderPipelineFrame and RenderBenchmark. Shadow views currently remove replaced persistent primitives but do not append transient SceneItems. ScenePipelineViews separately repeats built-in usages for drawing, ray-pick eligibility and legacy exclusions.

Retained views compare collection and prepared environments, and queued graphs retain copy-on-write snapshots. `FRenderViewStatistics` is reflected with only identity, usage and visibility. Its live consumers aggregate MainView and shadow benchmarks; diagnostic record shape and CSV columns must remain compatible.

## Goals / Non-Goals

**Goals:** Explicit small view policies, correct policy-sensitive cache behavior and owned statistics classification; one finite built-in route description; unchanged built-in draw, pick, transient and statistics output.

**Non-Goals:** Closing Usage to an enum, registering arbitrary render pipelines dynamically, generating RenderGraph stages, new passes, changing feature/plugin lifecycle, changing scene registration or diagnostic schemas, and replacing unrelated GPU timing pass-name protocols.

## Decisions

### M03A: View policy is an owned value independent of Usage

Add a small policy at the end of `FRenderView` with independently named transient-addition and replacement flags and a statistics category (Main, Shadow or uncounted). Ordinary view defaults preserve existing non-shadow behavior. A named shadow policy factory selects no additions, applied replacements and shadow statistics. Producers explicitly choose the policy; consumers do not infer it from Usage or from a role enum.

Migrate CSM construction and every in-repository manually constructed shadow fixture/view. A custom Usage can request the same shadow behavior without spelling `ShadowDepth`. Conversely, selecting a material pass does not implicitly change participation. Keep all shipped material Usage strings and lookup behavior unchanged.

In `AppendTransientSceneItems`, replacement filtering and additions are distinct actions. Preserve the current shadow deletion-only path and visible-item statistics, not an early return that skips deletion. In PrepareView, bypass retained collection only when transient data has an enabled effect. Do not put transient primitives into persistent scene registration or cause full-scene invalidation after overlays disappear.

### Cache comparisons follow the effect, snapshots own classification

Include transient policy fields in every collection/preparation compatibility comparison whose output they affect. A view with identical Identity/Usage/revisions but changed addition/replacement policy must not reuse incompatible membership or local preparation. Preserve copy-on-write when previously queued snapshots are still referenced, and preserve static reuse when inputs and policy are unchanged.

Statistics category does not alter draw/material preparation. Copy it with the owned view into each runtime `FRenderViewStatistics` result, including deferred preparation publication. MainView and shadow CPU benchmark accumulation read that category. A category-only change updates live classification without unnecessarily invalidating material contents. Keep family-wide compatibility totals in `FRenderSession::Statistics` unchanged.

The internal classification is not a new reflected property: existing diagnostics still emit identity, usage and visibility with unchanged IDs/versions/shapes. Internal runtime aggregation uses the captured classification; wire consumers continue to receive the existing raw view diagnostics, and no new wire round-trip guarantee for an untransmitted policy is claimed. Current in-repository MainView consumers use live statistics; verify this during implementation. Preserve existing CSV headers and the separate GPU timing `Pass.Name` prefix protocol, which identifies actual GPU stages rather than material usages.

### M03B: A finite Renderer-owned route description

Add a focused private descriptor inventory for the five scene routes: DeferredBase, HdrForwardOpaque, HdrCompatibility, HdrTransparent and Forward legacy. Associate a stable internal route identity with its unchanged Usage string, supported pipeline(s), pick eligibility and whether it excludes a material from legacy fallback. This describes current built-in routing only; custom material pass names remain legal and unknown routes are not registered dynamically.

Drawing and picking consume the same description with different queries. ScenePipelineViews stamps the selected route's Usage/eligibility while continuing to construct targets, depth, shadow bindings, indices and execution order explicitly at their current phases. Do not drive target creation through opaque callbacks or let a table reorder GBuffer, compatibility, transparent and display passes. Picking derives only eligible material usages and exclusions; it never schedules every drawing stage or includes shadow/fullscreen passes.

Legacy exclusion is the union of all four HDR usages, regardless of the currently selected pipeline. A Deferred-only material with a Forward fallback remains excluded from legacy drawing/picking under Forward, just as today; do not derive exclusions only from active routes. Keep the original ordered pick usage lists where they affect selection semantics. Stable builtin view identities and BaseIndex/TransparentIndex meanings remain unchanged.

### Acceptance and boundaries

Before migration run existing transient/material, retained-view, ray-query and pipeline tests and extend independent fixed matrices where original behavior is observable. M03A adds custom shadow Usage, the inverse case (ordinary policy despite a shadow-looking label), every addition/replacement flag combination, policy-only changes under stable identity/revision, clear/reuse, category-only changes and old snapshot immutability. Inspect actual membership, resource/source identity and statistics, not only helper return values.

M03B pins independent expected draw and pick matrices for legacy-only, each HDR usage plus Forward, mixed HDR passes, shadow-only and custom usages. Include the inactive-HDR legacy exclusion case. Existing Deferred/Forward pixels, transparency order, CSM, selection outlines, stable-frame caches and moving incremental membership tests remain relevant. Fixed diagnostic wire/schema and benchmark-column checks prevent incidental reporting changes.

Use affected Debug/Release targets for transient_materials, scene_spatial_visibility, instance_batching, deferred_rendering, cascaded_shadow_rendering, scene_ray_queries and selection_outlines, plus new focused cases. No new user-facing capability or automation operation is introduced; existing editor/agent rendering consumes the same shared Renderer path. Update rendering docs without adding historical local counts. Run style/naming, boundaries and strict OpenSpec validation, then independent final review.

## Risks / Trade-offs

- [Treating shadow as no transient work loses replacements] → Two explicit flags and deletion-only regression.
- [Policy changes hit stale retained views] → Effect-specific comparisons, same-key negative cases and old-snapshot assertions.
- [Statistics metadata borrows mutable views] → Copy category into both immediate/deferred owned results and preserve wire projection.
- [Route table broadens legacy fallback] → Derive exclusions from the complete inventory, with inactive-route fixtures.
- [An abstract scheduler obscures resource ordering] → Keep existing explicit target/stage construction; centralize only route facts.
- [Usage compatibility accidentally becomes closed] → Keep strings open and test custom usages with independent policies.

## Migration Plan

Capture fixed behavior, implement and accept M03A, then migrate and accept M03B. Rebuild affected CPU/GPU tests, review the combined final snapshot and commit locally. No persisted data or protocol migration; source rollback restores former branching.

## Open Questions

None blocking. Type/helper names remain implementation choices; the two transient decisions, effect-sensitive cache rules, owned classification and complete legacy exclusion set are fixed requirements.
