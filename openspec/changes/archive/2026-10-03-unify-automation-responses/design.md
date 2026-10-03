## Context

Sessions own typed Running/Completed/Failed/Cancelled states. Their envelopes, direct operation outcomes and connection completion envelopes share status tokens, while successful bootstrap catalog queries return unwrapped objects. CLI, MCP and connection code currently inspect these shapes independently. MCP uses the outer failed status for isError; cancelled job envelopes retain isError=false while their failure outcome becomes a nonzero one-shot CLI result.

## Goals / Non-Goals

**Goals:** One response codec and structural reader, unchanged valid JSON and execution semantics, controlled malformed-response errors, compatibility evidence through real consumers.

**Non-Goals:** No new operations, reflection schemas, protocol version, provider, response budgets, retries, fallback, domain branches or job lifecycle rewrite. No archive, commit or push.

## Decisions

### A small Runtime/Automation response contract

Add Response.h/.cpp with shared EAutomationStatus, completed/failure/job builders and ReadAutomationResponse. Session state uses the same four-value status type; its retention/admission/cancellation logic stays in Session. Keep result encoding admission in Jobs with its existing 1 MiB payload check and result_unavailable diagnostic; a generic completion builder must not impose that payload policy on connection/bootstrap responses.

The reader returns borrowed typed views over an unchanged FArchiveNode. Status is absent for unwrapped bootstrap objects; a completed/failed operation has result/error; jobs additionally carry identity, operation, cancellation and polling or terminal outcome. Views document that the source must remain alive and unmodified; reject rvalue construction. Avoid copying large payloads or introducing a general JSON-schema framework.

### Validate structure and relationships at the boundary

Require objects, known status strings and correctly typed required fields. Running jobs require a nonempty job/operation, bool cancellation and positive uint32 pollAfterMs, with no outcome. Terminal jobs have cancellable=false and an outcome consistent with completed/failed/cancelled status; cancelled outcomes retain the cancelled error code. Direct outcomes cannot masquerade as running/cancelled jobs. Reject conflicting reserved fields; tolerate unrelated additive metadata rather than rejecting future annotations. Error code/message/path remain strings (including empty strings), and details may be any archive value.

Read nested outcome once as a direct outcome; do not recursively accept nested job chains. Throw FAutomationError(protocol_error) for malformed envelopes. Keep operation payloads opaque: a result object's own status field belongs to that operation and must not be reinterpreted.

### Migrate each owner without changing its policy

- Jobs and connection server/manager use the shared builders. AutomationFailure keeps its existing public availability; move only its construction into the shared codec.
- ConnectionManager validates incoming results before publishing them and requires completed handshake payloads. Existing Close/pending-failure behavior handles malformed replies, without replay or local fallback.
- MCP and stdio use the typed reader for failure classification. MCP reports malformed internal replies as controlled internal protocol errors and preserves valid outer isError semantics, including cancelled jobs.
- One-shot CLI reads running job IDs and terminal outcomes through the shared view, preserving its existing output/exit behavior and response serialization budgets. JSONL and attach consume the same contract; outer correlation and JSON-RPC framing remain transport-owned.

### Verify before and after migration

Add fixed literal snapshots to the existing job tests and run them against original production code before migrating. Normalize only ephemeral job identity. Cover all terminal states, direct failure and completed outcomes, raw bootstrap objects, malformed/contradictory states, required field types, additive metadata and bounded nested outcomes. Test MCP immediate/deferred errors and memory-transport malformed handshakes/replies. Run existing real CLI/MCP, attachment, discovery, plugin availability and result-budget tests in Debug/Release.

## Risks / Trade-offs

- [Raw bootstrap results accidentally wrapped] -> explicit status-absent view and snapshots; preserve all producer shapes.
- [Cancelled jobs or nested failures change frontend behavior] -> fixed outer MCP flags and final CLI outcome tests.
- [Borrowed view outlives/moves its node] -> lvalue-only API, documented lifetime and copy selected outcome before replacing the source.
- [Validation causes successful mutation replay] -> retain no-retry/fallback contract; malformed results become explicit failures with unknown-effect guidance.
- [Unification changes payload/frame limits] -> keep existing policy checks in their owners and retain oversized-result regressions.

## Migration Plan

Capture snapshots and existing CPU/transport baseline, add the response contract, migrate producers and consumers, add malformed-response tests, validate both configurations and record evidence. No persisted data migration. Existing changes remain untouched; all three OpenSpec changes remain active.

## Open Questions

None.
