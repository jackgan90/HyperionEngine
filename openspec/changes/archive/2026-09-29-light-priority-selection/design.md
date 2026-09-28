## Context

Scene metadata already publishes all directional/sky components, but settings select one light of each kind. Builtin shaders combine one directional term with indirect and emissive terms. Editor activation actions edit settings and sky load status is separate text. Component operations already expose reflected fields with shared transactions.

## Goals / Non-Goals

Goals: one signed Priority property per light, deterministic derived selection, additive direct light across builtin paths, one shadow source, contextual diagnostics, GUI/automation parity and verified current assets.

Non-goals: multiple directional shadow maps, local environments, sky blending, old selection/API appearance compatibility, archive or commit.

## Decisions

- Scene owns a pure CPU selector using persistent object IDs, priority and eligibility. Highest priority wins; lexical persistent ID resolves ties. Publish the resolved result with immutable metadata and use the same resolver for Main diagnostics.
- Sky candidates require effective enablement only. Source readiness, zero intensity and background visibility do not affect selection. Directional shadow candidates additionally require cast shadows and nonzero radiance. CSM and contact shadows share the winner. Disabled per-method quality settings suppress that method without choosing another light.
- Remove authored light references from scene settings and native snapshots. Missing priority defaults to zero; obsolete selection keys do not drive rendering. Do not disable old unselected lights for compatibility.
- Render directional data as a separate immutable structured buffer/global list, not clustered-cell entries. Keep selected shadow direction/color semantics for shadow consumers; evaluate other direct terms independently so indirect/emissive are added exactly once. Neutral/default resources support non-scene material consumers.
- GUI Priority fields use reflection and existing component edit transactions. Display only highest-priority conflict warnings and hover diagnostics. Asset field presentation accepts contextual diagnostics without sky branches in generic Gui. Ready is silent; pending is transient; all load/dependency/upload failures remain observable.
- Replace light.main operations with per-component authoring plus typed resolved-lighting diagnostics. Remove the old provider where unused. Selection and diagnostic queries do not mutate history. Transport stays domain-independent.
- Sky Priority hover keeps its normal explanatory first line, then shows `Current effective environment` in green or `Environment overridden by <display name>` in red. An overridden enabled sky receives a normal-text suggestion to increase Priority above the other skies. Disabled skies retain their disabled explanation. Semantic tooltip lines are presentation-only; persistent IDs and tie-breaking details stay out of sky tooltip prose without changing the resolver or structured tie diagnostics.

- Both light types draw the highest-priority `[!]` warning as an independent hover target explaining that multiple eligible lights share the highest Priority, with no internal ID rules. Directional Priority help uses green for the current shadow source and red for another eligible light overridden by the winner's display name, with direct-lighting context and advice to increase Priority. Disabled or shadow-ineligible lights retain neutral exclusion explanations. GUI uses the shared shadow-eligibility predicate; selection and automation operations are unchanged.

## Risks / Trade-offs

- Extra unshadowed lights leak through occluders → expose actual shadow source and document one-shadow budget.
- Equal priorities are common → stable ordering and contextual conflict indication, no load-order selection.
- Old additional lights may become visible → explicitly accepted early-development breaking behavior; validate current assets rather than add migration modes.
- Resource or asset completion changes frame state → retain existing immutable sources and frame lifetimes; never let readiness select a different environment.
- Different builtin paths diverge → shared HLSL implementation and GPU checks with two differently colored directions, including no shadow candidate.

## Migration Plan

Default new priority fields to zero, remove authored selection fields from latest schemas, update fixtures/consumers/docs, and verify existing single-light content loads. No external asset rewrite unless necessary. Revert source changes to roll back; leave this change active after validation.

## Open Questions

None blocking. Exact diagnostic and buffer wiring follow existing engine-owned interfaces.
