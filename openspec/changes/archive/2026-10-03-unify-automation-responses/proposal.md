## Why

Automation sessions already own typed job states, but CLI, MCP and connection management separately interpret status strings and job/outcome structure. Completion envelopes are also constructed in multiple places, so protocol edits can silently diverge between entry points.

## What Changes

- Add a Runtime/Automation response contract for completed/failed outcomes, running/terminal jobs and unwrapped bootstrap query objects.
- Centralize response construction, structural validation and borrowed typed reading; migrate sessions, connections, MCP and stdio consumers.
- Preserve all valid JSON shapes, status tokens, operation schemas, response budgets, MCP flags and CLI exit/wait behavior.
- Report malformed peer responses as controlled protocol failures without retry, replay or target fallback.
- Add original envelope snapshots, malformed-response tests and consumer/lifecycle regressions.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `engine-automation`: One authoritative response boundary with compatible CLI/MCP and job projections.
- `automation-transport`: Validate incoming response structure before publishing it to callers or accepting a handshake.

## Impact

Runtime/Automation, the existing automation stdio plugin, focused tests and documentation. No new command, domain operation, transport provider, plugin dependency or lifecycle policy. Keep this change separate from the completed preview and Environment changes, active and uncommitted.
