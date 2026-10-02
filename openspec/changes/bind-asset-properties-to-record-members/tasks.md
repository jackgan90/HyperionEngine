## 1. Freeze current contracts

- [x] 1.1 Recheck record declaration/registry and asset property registration call sites, coordinate file ownership and record the actual baseline.
- [x] 1.2 Capture affected operation IDs, versions and schemas; add independent distinct-value property expectations that execute on the original registration path and run the relevant baseline.

## 2. Bind typed members to canonical fields

- [x] 2.1 Add optional safe typed correspondence to the existing Member declarations and unique typed lookup with owning-type, null, missing and ambiguity checks.
- [x] 2.2 Return owned canonical identity tied to existing descriptor definition lifetime; preserve custom callback fields, copied descriptor registration and conflict rejection.
- [x] 2.3 Test same-value-type members, wrong owners, unregistered/null/ambiguous members, custom fields, copies/lifetimes and altered association conflicts without representation-based comparisons.

## 3. Migrate asset operation registration

- [x] 3.1 Remove the independent field-target parameter from RegisterField and capture its derived canonical identity for get, set and sequence replacement.
- [x] 3.2 Verify all affected IDs/versions/schemas, distinct same-type get outputs, read-only set absence, paging, offsets, stale generation and shared asynchronous workflow behavior.
- [x] 3.3 Update reflection/automation documentation to distinguish typed member identity from external task naming; do not change editing policy or preview effects.

## 4. Validate and independently review

- [x] 4.1 Rebuild and run Debug/Release reflection, automation asset and relevant editor document/workspace consumers, preserving archive byte and persistence compatibility.
- [x] 4.2 Rebuild required executables and exercise existing CLI/MCP discovery/invocation parity and stale/drain/document-replacement regression paths; record exact environment limits.
- [x] 4.3 Run applicable style/naming, module boundaries, diff and strict OpenSpec checks, obtain independent frozen-file review, verify findings directly and resolve scoped issues.
- [x] 4.4 Report the accepted M11A snapshot and remaining M11B scope to the orchestrator for accurate ledger and separately scoped local commit; do not push.
