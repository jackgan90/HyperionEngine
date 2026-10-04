## Context

EditorOptions validates `--exercise-capture` against exactly four nonempty tokens, then stores a string. Empty means no request. The same field controls GUI persistence isolation, harness activation, screenshot timing and exercise dispatch. It is private startup state, not a reflected or persisted record. BUILD_TESTING=OFF rejects all exercise arguments before value parsing.

## Goals / Non-Goals

**Goals:** Typed capture exercise identity, one authoritative CLI/log mapping and unchanged acceptance behavior.

**Non-Goals:** A generic option registry, other exercise fields, new automation exposure, RenderDoc/provider lifecycle changes, scenario redesign or document-close status changes.

## Decisions

1. Define `EEditorCaptureExercise` with Toggle, Capture, Unavailable and Hud beside private EditorOptions. Store `std::optional<EEditorCaptureExercise>` so no-request is explicit. A sentinel enum member would conflate absence with an executable mode; retaining opaque strings is unnecessary because this CLI is already closed.
2. Keep one private named mapping in EditorOptions.cpp. ParseEditorCaptureExercise accepts only the existing exact tokens and preserves the current error message. EditorCaptureExerciseName converts valid enum values for logs and rejects invalid native enum values. No ordinal indexing or parallel text cache is needed.
3. Replace all presence and equality predicates with optional/enum predicates in their current locations. Keep CLI missing-value handling, left-to-right duplicate behavior, benchmark validation precedence, isolated preference-path check and BUILD_TESTING rejection unchanged. Default options remain inactive and retain normal GUI persistence.
4. Keep scenario methods and state in the existing acceptance harness. The closed enum and boundary conversion do not introduce scenario state into production headers. Existing GUI and automation operations remain untouched; this change adds no user capability or adapter requirement.
5. Add a focused private options executable using the actual Editor plugin implementation. Cover all four modes, absence, duplicate arguments, rejected spellings, isolated preference policy and existing error precedence. Reuse actual capture UI acceptance for execution/log/persistence equivalence and independently verify the production build boundary.

## Risks / Trade-offs

- Presence conversion could alter GUI persistence or timeout activation → test default and every mode, and inspect all field references.
- Log conversion could change external test markers → reuse the same mapping and existing exact-marker UI tests.
- Production/test or optional RenderDoc boundaries could drift → retain preprocessor ordering, compile/link affected configurations and run source-boundary/CLI checks.

## Migration Plan

Capture existing CLI and GUI behavior, implement the private type and consumers, build and run focused tests, compare baseline CLI results, perform independent quality audit and stop for user diff acceptance. No data migration is required.
