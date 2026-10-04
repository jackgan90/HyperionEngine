## Context

The baseline is ba3f17b. The previous close-observation change is implemented but remains uncommitted. The user replaced small acceptance checkpoints with four batches: items 4/9/10/12, then 13, then 14, then 15/16. This change implements the first batch and stops only after all its implementation, validation and independent review.

Assets discovery, Editor browsing and AssetTool migration repeat reserved entry names. RasterOptions already owns pipeline/preset/visualizer/shadow identities, but Config and Renderer retain encoded values and decode them repeatedly. Six domain exception families carry codes; Reflection wire errors instead carry a path. Materials owns pass definitions while Renderer currently infers fallback silhouette coverage from shader paths.

## Goals / Non-Goals

**Goals:** Complete the four related contract cleanups with owner-defined semantics, compatible defaults and external values, meaningful regression coverage and one acceptance boundary.

**Non-Goals:** Perform all oversized-file cleanup, reorganize Renderer directories, restructure Editor controllers or Endpoint dispatch, change native rendering algorithms, redesign plugin lifetimes, or modify/commit the user draft. Unavoidable local splits follow the existing file/function policy without absorbing later batches.

## Decisions

### Assets owns reserved names; callers own filtering policy

Add a small Assets contract for reserved entry names and the publication staging prefix. Discovery and migration use exact case; Editor continues folding names before querying. Migration additionally excludes its legacy .assets and catalog files. Reuse the staging prefix in publication name construction. Content cannot own this contract because it already depends on Assets.

### Typed runtime values and explicit compatible boundaries

Use the existing RasterOptions enums in FAppSettings, FRenderSettings, scene pipeline and light-shadow state. Keep existing Config FProperty and record representations through mapped field adapters; a reusable Reflection member adapter preserves the wire type, options and actual C++ member association while applying owner-provided conversion. Do not expose numeric enum metadata where the old schema was an unrestricted integer/string. Preserve preflight, complete-candidate commits, revisions and persistence keys.

FSceneViewportOptions remains the explicit optional numeric request/response DTO, including unsupported/null behavior and unsupported-before-invalid validation order. It is decoded at the shared viewport boundary; runtime state and GUI presentation consume typed culling/outline/visualizer identities and typed HUD flags. Profiling HUD flags belong to Renderer and are distinct from Core trace collection categories. One option table defines their fixed bits and labels, replacing index-derived bits and the literal 255 mask.

Typed record conversion can report an invalid top-level option before a nested contact validation error when both are invalid. This deliberately permits a different first diagnostic message for multiply-invalid requests; their code/path/details, whole-request rejection, wire structure/width checks and provider/viewport service precedence remain unchanged. Single-invalid compatibility is checked separately. Preserving the exact choice among multiple semantic errors would require retaining raw invalid values or redesigning record validation phases, outside this cleanup.

RHI backend names remain configuration/provider-selection input: Config does not acquire a dependency on RHI. The native backend registry remains their owner and performs the existing single startup conversion. This is not a repeated render-state decoding path.

### Domain-owned open error identities

Introduce a small Core value type for a stable error identifier and a coded-exception base, without putting domain catalogs or a closed universal enum in Core. Each domain publishes its known identifiers once; internal throws and branches use them. An explicit external-code constructor retains unknown provider/protocol names losslessly. Preserve messages, paths, details and catch-family meaning.

Automation retains its richer error envelope and catches the common coded-error contract after its own error subtype and before generic exceptions. Share any remaining necessary domain adapter across invoke and deferred Poll. Remove redundant catch/rethrow wrappers only where they do not add operation-specific meaning, path/details or state cleanup. Transport and generic record decoding gain no domain branches. Wire tests retain fixed literal expectations; internal semantic tests use typed identifiers. Validate unknown-code forwarding, wire-path handling, synchronous and asynchronous failure and cancellation.

### Materials owns extensible names and declared coverage

Define well-known usage/variant names in Materials public contracts; retain string extensibility and fixed spellings. Existing execution-mode to HYP_ENABLE_INSTANCE handling is already centralized and remains unchanged.

Add an optional typed silhouette fallback policy to material passes: Disabled or ModelShader. Fresh authored passes default to Disabled; built-in PBR declares ModelShader. A missing/null value represents a legacy decode input and is resolved once while constructing the immutable material definition with the exact former shader eligibility rule. Renderer only consumes resolved policy and explicit SilhouetteMask passes; it no longer inspects source paths/entry names. Keep complete compiled parameter schemas and inactive overrides for auxiliary masks. Materials rejects ModelShader declarations lacking a pixel shader at definition/asset validation, before any selected rendering frame.

Persist material passes as version 2, migrate version 1 fields to an explicit policy using the old rule, and retain the same outer asset format. Reflected decode defaults preserve older wire inputs that omit the newly optional field. Existing operation IDs/versions remain unchanged; describe exposes only the compatible nested optional policy extension. Document its authoring boundary and deferred GUI/automation setter if the existing pass-authoring surface does not expose edits. Validate old fixed fixtures, explicit disable/enable/custom-mask selection and publication/cache invalidation when policy changes. No mass rewrite of external asset repositories.

## Risks / Trade-offs

- Type changes can silently narrow values or alter schemas -> preserve wire widths/defaults and capture baseline contracts; check fixed invalid values and complete schema snapshots.
- Exception unification can change classification or discard details -> inventory existing throws/catches, preserve contextual adapters, cover synchronous/deferred/unknown errors and failure side effects.
- New coverage defaults can change old assets -> migrate legacy passes with the exact old predicate and test Model/custom/defined-shader cases; preserve old wire omission behavior explicitly.
- A cached material could keep an old coverage decision -> include policy in definition/material identity and exercise policy-only changes through shared assets and outline preparation.
- Broad batch scope increases review burden -> retain per-area evidence/tasks and freeze one final manifest for fresh independent reviewers; do not add user acceptance stops between areas.

## Migration Plan

Capture existing operation/configuration and legacy material fixtures before production edits. Implement all four owners and their consumers, update docs/specs, run focused native/GPU/GUI/automation compatibility checks and production builds. Freeze the complete scope for independent quality-audit, verify and minimally fix confirmed findings, then present the batch for user acceptance. Archive and commit remain a later accepted-scope action. New native material-pass records require the new reader; retain old source assets and avoid in-place bulk migration during this work.
