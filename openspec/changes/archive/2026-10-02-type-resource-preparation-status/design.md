## Context

Editor owns placement resources and lifecycle. Its private FPlacementService already shares model preparation and document creation between GUI and automation, while Editor currently combines model, preview material and icon availability as strings. Renderer owns sky asset loading, RHI texture preparation and Main publication; reflected lighting diagnostics expose string state values.

## Goals / Non-Goals

**Goals:** Explicit state and stage, one placement admission rule, message-independent consumers, stable external schemas and completion semantics, and direct regressions for the old coupling.

**Non-Goals:** New placement features, new transports or plugin dependencies, a universal asynchronous state framework, changes to material fallback policy, asset formats, sky selection or GPU retirement.

## Decisions

### Placement result and shared aggregation

Use an Editor-private preparation value with Ready/Pending/Failed state, a stage (scene, model loading, model upload, preview material, icon loading/upload) and independent error. FPlacementService combines its model state with a Main snapshot supplied by Editor for scene validity, preview material and the relevant icon. Keep the current scene-first and model-before-preview admission order. GUI and automation inspect the result; final Commit rechecks through the same rule. A presentation formatter produces palette/catalog/drop messages but never supplies state to business logic.

Resource owners, polling tasks and reset/drain ordering stay where they are. Icon completion becomes an explicit typed value; CPU completion alone does not mark an icon ready. Model snapshots include load completion so a completed load without data fails even with an empty exception message. Native resource/material enums remain the authority for GPU preparation. Native source materials remain optional for transient previews: ready sections use them and pending sections retain the existing fallback.

Only renaming prefixes would preserve the hidden protocol. Moving all placement resources into a new Runtime module would add unnecessary lifecycle and dependency changes; small owned snapshots give the shared service all admission facts without relocating resources.

### Sky state and string reflection boundary

FSceneSkyStatus uses an enum for None, Unrequested, Loading, Uploading, Ready and Failed. FSkyLoad stores explicit Loading/Uploading/Ready/Failed state; success and catch paths publish terminal states. Pending/failed counts derive from state, independently of error text. Existing task submission and retirement flags retain their task-management role.

A Renderer-owned reflection member uses existing FRecordMember callbacks and a string value shape to map states to the exact existing values: empty string, unrequested, loading, uploading, ready and failed. Keep the record ID, revision, state/error keys, default values and schema unchanged. Do not use default enum reflection, which encodes numbers, and do not change generic Reflection. Unknown strings are rejected explicitly rather than silently becoming a valid state.

Asset previews and light tooltips inspect typed state; GetSkyStatus formats display text. Preserve previous sky data on failed replacement, the selected sky's failure behavior, explicit retries and all cancellation/drain rules.

## Risks / Trade-offs

- Admission drift from aggregation changes -> preserve gate order, fallback policy and required resource conditions; cover model/material/icon stages independently.
- Numeric enum wire conversion or changing non-sky empty states -> use the local string codec with fixed wire/schema fixtures, including the None state.
- Tests sharing message parsing -> migrate acceptance control flow to typed state and add fixed collision/error fixtures independent of formatting.
- Empty error text hiding terminal failures -> use terminal state/completion information and format a fallback diagnostic without changing error classification.
- Lifetimes weakened during cleanup -> no resource relocation; exercise cancellation, stale requests, scene/root replacement and shutdown regressions.

## Migration Plan

Add the typed values and regression coverage, migrate shared admission and consumers, then validate serialization and real attached CLI/MCP operations. Update current module documentation. No persisted data migration is needed; reverting the source change restores the former implementation without rewriting assets.

## Open Questions

None required before implementation. Validation will record actual available GPU/build configurations and any environment limitations.
