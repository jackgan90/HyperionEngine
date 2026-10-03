## Context

`FConnectionManager::Resolve` searches `ITargetDiscovery::List`. Local enumeration stops at 128 candidates or 1024 entries; stale legacy registrations therefore hide an exact known instance. Registration is advisory and the current-user transport admission and expected-instance handshake remain authoritative. Terminated acceptance hosts may leave registrations in the user's directory.

## Goals / Non-Goals

Goals: resolve known IDs without enumeration; retain bounded, truthful listing; prove process ownership before stale filtering/deletion; isolate acceptance children; preserve direct-address and handshake behavior.

Non-goals: pagination, automatic removal of legacy/unknown records, heartbeat/lease protocols, background polling, protocol changes, remote process assumptions, or changes to domain operations.

## Decisions

1. Add `Find(instance)` to `ITargetDiscovery`. Local lookup validates the 32 lowercase hexadecimal identity and reads only its bounded regular record, independent of `List`. Both probe and connect share this route, including CLI `--attach`. A missing record never selects another target.
2. Return a typed list snapshot with targets, examined count, stale-skipped count, unknown-ownership count and stop reason (`complete`, `result_limit`, `scan_limit`). Following existing reflected enum wire conventions, their stable values are 0, 1 and 2; names come from enum metadata. The connection adapter derives `truncated` from the reason and retains `targets` and `contract`. Limits remain 128 results / 1024 examined entries; report a limit only if unexamined entries remain. Enumeration does not delete files.
3. Keep `FAutomationTarget` and handshake schemas unchanged. New local files use a private version-1 envelope containing target plus process ID and process creation identity. Read legacy bare target records as unknown ownership. Reject malformed, oversized, mismatched-name and unsupported-version records without deletion.
4. A private Windows adapter compares PID plus creation time and nonblocking process status. It returns alive, dead or unknown; access/query failures mean unknown. PID reuse is dead for the recorded identity. Unknown ownership remains eligible for the normal handshake. Transport absence/timeouts never prove death. Native APIs stay private to Automation.
5. Publish performs bounded opportunistic cleanup of proven-dead versioned records. Deletion opens the record without write/delete sharing, rejects reparse points, rereads and compares the exact bytes, then marks that same handle for deletion. Changed, locked or unknown records survive. Cleanup failure does not prevent publishing a healthy target. Normal withdrawal remains plugin-owned; no per-frame maintenance is added.
6. Python acceptance launches share one unique per-run `LOCALAPPDATA`, inherited by Editor, CLI and MCP. A reusable command wrapper covers CTest launches, including the C++ lifecycle publisher. Shared Python helpers also isolate standalone runs. Existing explicit failure-environment overrides remain effective. Only per-run fixture directories may be cleaned.

## Risks / Trade-offs

- Older frontends cannot read the private envelope: upgrade host and frontend together; the new reader accepts legacy files.
- Unknown legacy records can still fill advisory listings: truthful truncation and exact lookup handle this without unsafe deletion.
- Bounded maintenance may leave stale files beyond the scan budget: no availability dependency is placed on cleanup completing.
- Enumeration sees a changing directory: counts describe one bounded attempt, not an atomic global snapshot. Identity is rechecked on each connection.
- Process creation identity and safe file deletion are platform-specific: isolate them behind private functions and test both native behavior and deterministic unknown/dead cases.

## Migration Plan

No conversion or bulk deletion. New hosts publish version 1; new readers accept both formats. Document response metadata and upgrade pairing. Validate saturation beyond both limits, PID reuse/death/unknown ownership, safe deletion and real Editor/CLI/MCP attachment in isolated discovery environments. Leave this change active and uncommitted after testing.
