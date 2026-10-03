## Context

ImportTaskInfo stores running/completed/failed, import draft storage and snapshots use preparing/ready/publishing/failed/discarded, and AutomationSession privately stores running/completed/failed/cancelled. Editor uses draft and task snapshots directly. Default empty draft snapshots are used before a preview exists. Existing string fields are reflected and externally discoverable.

## Goals / Non-Goals

Goals: typed decisions throughout these three lifecycles, one explicit token mapping per domain, unchanged observable success/failure/poll/cancel/root-retirement behavior and unchanged string schema shape.

Non-goals: a generic state-machine framework, new cancellation support, new operations, changing history actions or the wider Editor transition state machine.

## Decisions

1. AssetImport owns EImportTaskState and EImportDraftState. Both persistent runtime tracking and DTO snapshots use enums directly, so callers do not parse display strings back into states. Draft None maps to the existing empty default; it is not an admitted workspace draft state. Task defaults to Running, workspace drafts to Preparing. Keep Status field spelling and serialized field IDs.
2. Adapt only the two reflected status members to string read/write/shape, preserving descriptions, association and all other metadata. Explicit value/token tables serve encoding and decoding; reject unknown tokens and invalid enum values. Do not use default numeric enum reflection or add enum constraints to the existing string schemas. The mapping helper remains private to AssetImport; no new Reflection feature is needed.
3. AutomationSession owns a private EJobState. Convert to the existing status strings only when constructing protocol envelopes. Cancellation remains a terminal job observation; provider draining remains a separate lifecycle responsibility.
4. Keep transitions at existing domain mutation sites. In particular, a publication failure returns a prepared draft to Ready with Error and does not update SavedKey; preparation failures remain Failed. Preserve bounded task/job retention, generation changes and content-root busy/dirty checks.

## Risks / Trade-offs

- Numeric enum encoding would change schemas: use explicit string members and fixed expected-schema tests, plus round trips and unknown-token rejection.
- Default preview snapshots have no state: retain None/empty instead of accidentally displaying preparing or failed.
- Same spelling does not imply the same domain state: use distinct enum types and explicit comparisons; do not unify task and draft transitions.

## Migration Plan

Record existing tests, add types and adapters, migrate every C++ consumer, run focused lifecycle/wire tests and Debug/Release builds, then independent review. Existing serialized/wire records require no migration. Leave OpenSpec active and all code uncommitted.

## Open Questions

None.
