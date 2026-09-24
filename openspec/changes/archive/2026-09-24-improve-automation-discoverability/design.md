## Context

The 2026-09-24 supervised run exercised 502 MCP requests without source knowledge. Core domain execution works, but several metadata/discovery gaps force guesses. Existing Reflection value shapes, Content mounted ListDirectory, AssetEditing transactions and typed host providers are sufficient. AssetAutomation::Describe omits workspace active state while DescribeWorkspace adds it.

## Goals / Non-Goals

Goals: remove demonstrated discovery blockers; make semantic values and nested schemas usable without source; return consistent document state; reduce diagnostic and target-selection friction; preserve GUI/agent shared validation, history and persistence.
Non-goals: transport/framework redesign, arbitrary filesystem access, remote transport/authentication, embedding search, generic query languages, automatic retries or weakening revision checks.

## Decisions

1. Resolve workspace-owned state in the shared automation description path so mutation/query results agree. Regression covers active and inactive documents, including undo/save results.
2. Catalog type registration traverses persistent record value shapes (including optional/container children), with cycle protection and existing duplicate-ID checks. Metadata lifetime follows current descriptor ownership. Do not maintain a second manual nested-type list.
3. Add reusable enum labels/descriptions to reflection metadata, keeping numeric wire/persistence values compatible. Export labels and descriptions in schema and readable descriptions for clients that flatten JSON Schema. Populate relevant existing enum domains. Field semantics remain beside domain record registration.
4. Add semantic numeric material get/set operations, converting finite typed values in AssetEditing into existing material parameters and committing through the same shared CommitAssetField transaction used by GUI. Preserve raw words operation. Validate shape, scalar range and boolean domain before mutation.
5. Add a paged direct-child mounted directory query through Runtime/Content over existing IFileSystem::ListDirectory. Return directories and unrecognized files plus errors; distinguish indexed asset identity from unindexed candidates without falsely diagnosing all unindexed files as corrupt. Per-file open remains authoritative. Preserve generation, mounted path, link and root boundaries.
6. Document standalone versus workspace retention in operation descriptions and expose a queryable workspace policy. Do not force an Editor-only failed tab lifecycle into standalone.
7. Add compact application health using existing typed host provider status, with readiness/error/host context; retain full render statistics. No render details leak into Runtime/Automation.
8. Add optional target labels/mode metadata via host composition and bounded explicit connection-layer probing of a chosen candidate. Probe uses existing transport/handshake, is advisory, and does not replace identity validation or implicitly attach a domain session. Avoid automatically probing all stale candidates or deleting records.
9. Include operation descriptions in deterministic token search; add focused keywords and bootstrap range text. Preserve IDs and existing AND token matching, with no aliases or external search dependencies.
10. Contract tests check nested lookup, semantic schema metadata, input examples, material conversion/atomicity and response/query consistency. Integration tests use fresh binaries and isolated preferences/content. Existing optional-provider behavior remains controlled.

## Risks / Trade-offs

- Recursive metadata -> traverse each ID once and retain duplicate descriptor compatibility checks.
- Added enum metadata maintenance -> define reusable metadata per enum, derive validation values where practical; do not duplicate values per operation.
- Directory contents change between pages -> document non-snapshot pagination and root generation; restart after edits. Listing does not imply full native payload validation.
- Probe can time out on busy applications -> expose bounded failure as unavailable/unknown advisory state, never proof of process death.
- Numeric conversion -> reject nonfinite, out-of-range, fractional integers and invalid booleans before any transaction.
- Existing activated MCP process holds older binary -> validate via fresh process from separate build output when necessary; do not claim old session refreshed.

## Migration Plan

Only additive operations/fields/metadata and correction of active state. Existing operation IDs, numeric enums and raw material words remain compatible. No asset file migration. Update contributor docs, build/tests and OpenSpec tasks; leave uncommitted and unarchived for user review.

## Open Questions

None requiring user decisions. Exact provider field names and regression placement follow existing code during implementation.
