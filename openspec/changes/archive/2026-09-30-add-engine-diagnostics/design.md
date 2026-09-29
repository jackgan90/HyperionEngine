## Context

Core Log already feeds the Editor history and typed `application.log.read`. Several services consume exceptions into error/status values, so entrypoint logging cannot see them. D3D12 Statistics currently replays all errors on every query and omits warnings.

## Goals / Non-Goals

**Goals:** Explain critical lifecycle decisions and final async outcomes with stable operation labels, paths/identities and the original cause. Keep normal frame/update/query paths quiet and preserve current domain results.

**Non-Goals:** Per-frame, per-draw or per-property traces; logging every throw/catch; another logging framework; changes to error schemas, cancellation, transactions or transport. This is a focused coverage pass, not a promise to instrument every engine validation branch.

## Decisions

- Put diagnostics in the service that accepts or finalizes an operation, not both GUI and automation adapters. Use existing Core Log and existing log reads; no new reflected operation is needed.
- Info describes committed content transitions, import/save outcomes and a compact startup summary. Warning describes a recoverable degradation. Error describes a failed accepted operation or cleanup. Debug may describe accepted work before completion. Expected cancellation and repeated status reads remain quiet.
- Include quoted paths/IDs and explicit stage/outcome. Never imply that publication succeeded before it did, or that old state survived unless the code guarantees it. Preserve the original cause before a catch translates it.
- Log async failure only behind terminal-state transitions (`Pending` reset, complete/failed flags or failed scene state). Do not add logging to transform, draw, scheduler, cache lookup or ordinary polling paths.
- D3D12 collects warning/error/corruption text when inspecting the existing native queue, tracks inspected entries and suppresses identical severity/ID/text records for that device. Distinct diagnostics remain visible and validation error counts retain their current semantics.
- Cleanup logging is best effort within the existing noexcept path; failures of logging must not interrupt cleanup or replace the stored original exception.
- DXC diagnostics are logged at actual compilation, including warnings on successful compiles, with path, entry, shader profile, payload target, optimization and defines. Cache hits remain quiet; diagnostic exceptions retain their original text.

## Risks / Trade-offs

- Diagnostics emitted by multiple layers can duplicate facts → assign messages to terminal domain boundaries and avoid UI copies; a higher-level summary may add scene/operation context.
- Native diagnostic text may repeat on frequent statistics calls → remember processed queue entries and previously reported diagnostic keys.
- Async tests may pass before completion → wait for the domain terminal result, then poll repeatedly and assert log counts stay stable.

## Migration Plan

No persisted formats, plugin selection or operation IDs change. Build affected targets, run failure/absence and logging regressions, and retain the active change and uncommitted edits for review.
