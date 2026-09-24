# Validation

## Result

Completed all tasks for `improve-automation-discoverability`. Changes remain uncommitted and the change remains active (not archived).

## Build and static checks

- Full Debug build succeeded. Latest incremental verification also succeeded without MSVC warnings/errors in `out/AutomationOptimizationBuildVerified.log`.
- Whole-repository filename/include casing and formatting checks passed (888 owned source files).
- Semantic naming/declaration checks passed for 31 changed translation units; the final five adjusted units were checked again successfully.
- Dependency boundary check passed (852 scanned files, 42 modules).
- `git diff --check` and `openspec validate improve-automation-discoverability --strict` passed.

## Runtime checks

The final focused run passed 11/11 tests:

- automation_assets
- automation_contracts
- automation_connections
- automation_transport
- reflected_archives
- material_contracts
- material_bindings
- automation_attachment
- automation_capability_parity
- automation_parity_regressions
- material_asset_contracts

Five additional related tests passed in the preceding run and were unaffected by final refinements: automation_local_lifecycle, automation_scene, editor_asset_workspace, editor_asset_documents, editor_asset_editors. Thus 16 distinct related tests passed across the verification runs; this is not a claim of running the entire CTest suite.

The fresh-process CLI/MCP integration checks cover:

- Directory discovery returns an unknown invalid native file; opening its discovered path returns failure; standalone policy/list correctly report no retained failed tab.
- Root generation, page limits and package-root traversal validation.
- Every advertised nested record type in the attached Editor operation schemas can be independently described.
- Numeric material editing/readback, shared PBR clamping, invalid-tail atomicity and undo; unit tests also cover signed/unsigned/bool conversion, range errors, nonfinite values and preservation of the prior draft.
- Active and inactive workspace mutation responses agree with subsequent queries across connections.
- Compact health reports ModelViewer load failures and remains separate from full statistics.
- Repeated successful probes do not exhaust connections; stale identity, timeout and invalid limits are rejected. Probe results exclude temporary connection/session IDs.
- Real local transport discovery/probing/attachment, document edits, history, save/reopen, disconnect isolation, optional listener disablement and startup failure.
- Existing material serialization and reflection roundtrips.

## Corrections during verification

An initial Debug link encountered LNK1236 in a generated object/library; a rebuild succeeded. A new test comparison emitted signed/unsigned optional comparison warnings and was corrected. One existing integration assertion hardcoded ten bootstrap tools; it now checks the explicit eleven-tool inventory including targets.probe, and the rerun passed.

Evidence logs are generated artifacts under `out/AutomationOptimization*.log`; in particular `BuildVerified`, `TestsVerified`, `StyleVerified`, and `NamingVerified`. Test applications exited after verification.

## Scope and remaining boundaries

Validation uses newly launched Debug CLI/MCP and application processes. The already configured Release MCP process was not used as proof of new behavior and was not replaced; using the changes there requires rebuilding that configuration and restarting the process. No transport hot reload or remote transport is added.

Numeric convenience operations cover top-level scalar/vector/matrix Numeric parameters. Aggregate/resource parameters retain their existing typed values interface. Directory listing exposes candidates and access errors, not a new full asset validator or filesystem snapshot. Target label/mode describe startup configuration; probe results are advisory, and connect still validates identity.

The pre-existing untracked `editor-asset-tests/` and `shader-fixtures/` directories were left outside this change. No Git staging, commit or push was performed.

## Independent quality audit

The subsequent audit confirmed and repaired AD-01: partial enum labels and duplicate aliases could make schema reject values accepted by wire decoding. Schema now preserves all distinct legal values and combines alias metadata. The original independent reviewer re-reviewed the repair and reported no remaining finding. See [audit.md](audit.md) for scope, snapshot hashes, verification and the documented mixed-binary compatibility boundary.

After repair, a Debug build and all 11 focused tests above passed again (156.43 seconds). Whole-repository formatting/path and boundary checks, the two repair translation units' semantic naming checks, strict OpenSpec validation and diff whitespace checks passed. Fresh evidence is under `out/AutomationDiscoverabilityAudit/`, especially `BuildFinal.log`, `FinalTests.log`, `NamingVerified.log` and `EnumReproFixed.log`. This audit did not rebuild Release or archive/commit the change.
