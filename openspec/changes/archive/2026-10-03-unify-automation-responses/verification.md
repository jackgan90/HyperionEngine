# Verification — unify-automation-responses

2026-10-03, base `7f6ce3b` on `main`, alongside the active uncommitted `type-asset-preview-contracts` and `unify-environment-bake-validation` changes. Implementation and validation are complete; the change remains active. No archive, commit or push.

## Scope and compatibility

`Runtime/Automation/Response.h/.cpp` owns the status-to-wire mapping, completed/failure/job construction and borrowed structural views. Session uses that status identity while retaining its admission, polling, cancellation, retention and draining behavior. Jobs retains its original payload encoding check. Operation keeps the existing failure API available through its public include.

Jobs, ConnectionServer and ConnectionManager use shared builders. Stdio, MCP and ConnectionManager use shared typed reading. Connection correlation, framing, admission and timeout policy remain separate; malformed envelopes are rejected before publishing a remote result. A malformed reply closes the connection and fails remaining requests without fallback or replay. Valid raw bootstrap replies stay raw, operation payloads stay opaque, terminal job outcomes stay nested, and existing status/error field names and serialized values remain unchanged.

MCP keeps its outer-status `isError` rule, including false for cancelled jobs. One-shot CLI keeps waiting and emits the terminal outcome, whose failure determines its exit status. Error code/message/path/details, probe payload shape, result and frame budgets, operation IDs, reflection schemas and plugin lifecycle contracts are unchanged.

## Baseline and targeted coverage

Before production edits, added literal direct-failure and running/completed/failed/cancelled snapshots to JobStateTests.cpp. Only ephemeral job identity is normalized. The original implementation passed these snapshots and the existing `automation_contracts`, `automation_connections` and `automation_transport` baseline: 3/3, 18.23 s. Baseline logs: `out/maintainability/AutomationResponses/Baseline{Build,Tests}.log`.

- ResponseTests.cpp covers payload borrowing, raw bootstrap objects, opaque payload status/job fields, error details and paths, unrelated additive metadata, all task states, direct-completion access rules, malformed fields/statuses/relationships, polling overflow and bounded direct outcomes. Invalid internal status construction is rejected.
- McpResponseTests.cpp covers raw/completed/failed and all task states for immediate and deferred endpoints. Text and structured results agree; malformed replies produce `-32603` and the endpoint executes once.
- ResponseConnectionTests.cpp sends malformed or non-completed handshake replies and malformed routed responses over a 128-byte, 3-byte-fragment memory transport. It verifies failure propagation to two pending requests, connection closure, no repeated results, no replay/reconnect and preserved peer failure details.
- Existing tests cover fixed snapshots, session retention/cancellation/draining, fragmented shutdown replies, real JSONL/MCP and one-shot success/failure, oversized results after effects, large correlation IDs, plugin absence/disablement/startup failure, attachment/discovery, GUI/agent parity and asset previews.

## Builds and checks

Debug and Release built `automation_tests`, `automation_connection_tests`, `automation_asset_tests`, `automation_local_tests`, `transport_tests`, `hyperion_automation_cli` and `hyperion_editor` with MSVC 14.50.35717 through the repository's Visual Studio environment setup. BUILD_TESTING stayed ON; RenderDoc stayed ON for Debug and OFF for Release; Tracy stayed OFF. Final build logs contain no compiler warning/error matches.

```powershell
./tools/Build.ps1 -Preset debug -Target automation_tests
cmake --build out/build/debug --target automation_connection_tests automation_asset_tests automation_local_tests transport_tests hyperion_automation_cli hyperion_editor --parallel 8
ctest --test-dir out/build/debug --output-on-failure -R '^(automation_contracts|automation_connections|automation_transport|automation_attachment|automation_discovery|automation_capability_parity|automation_parity_regressions|automation_asset_preview|plugin_applications)$'
ctest --test-dir out/build/debug --output-on-failure -R '^(transport_contracts|automation_local_lifecycle)$'
# Repeat using release / out/build/release with the Visual Studio environment initialized.
```

Debug passed 9/9 integration/contract regressions (123.68 s) and 2/2 transport/lifecycle regressions (1.43 s). Release passed 9/9 integration/contract regressions (70.13 s) and 2/2 transport/lifecycle regressions (1.45 s). Logs are in `out/maintainability/AutomationResponses/{Debug,Release}{Build,Tests,LifecycleTests}.log`.

Full owned formatting/path checks passed (1105 files). Semantic naming and local-declaration checks passed on all 13 changed Automation translation units and included headers. Dependency checks passed (39 modules). `git diff --check` and strict OpenSpec validation passed. Changed production files are 45–356 physical lines; modified/new functions remain below 100 lines. Manual review checked authoritative status identity, bounded outcome parsing, borrowed-view lifetime at source replacement, additive fields versus conflicting envelope fields, preserved error/result budgets and no transport domain branches.

This is the affected regression set, not a full repository suite. Internal malformed MCP endpoints and malformed remote peers are synthetic tests; real frontend tests exercise the shipped protocols and providers. No unresolved validation failure remains.

## Quality-audit follow-up — 2026-10-03

Independent review identified AR-001 (P2): a valid completed envelope containing malformed hello payload data produced `invalid_arguments` or `operation_failed`. The underlying field reads predated this change; the defect was incomplete coverage of the new `protocol_error` guarantee. The main agent independently reproduced null/missing/wrong-type cases, then added a regression that failed against the pre-fix implementation.

The fix parses hello payloads at the connection boundary and converts malformed data to `protocol_error`, retaining opaque shared response payloads, peer failure details, `protocol_mismatch`, `stale_target`, connection closure and no replay. Added cases cover invalid object shape, missing fields, wrong field types, integer range and unchanged version/budget/identity policy failures. Debug and Release rebuilt successfully and passed six focused regressions each (57.54 s / 28.67 s): preview contracts, automation contracts/connections/transport/attachment/discovery. Formatting, boundaries, semantic naming and strict OpenSpec checks passed. The original independent reviewer relinked the harness, confirmed corrected errors and preserved policy/metadata behavior, and closed AR-001. Evidence: `out/QualityAudit/MaintainabilityFollowups/AuditReport.md`, `RedTest.log`, `{Debug,Release}Tests.log` and `Responses/RecheckResults.log`. No unresolved audit finding remains; archive and commit remain unperformed.
