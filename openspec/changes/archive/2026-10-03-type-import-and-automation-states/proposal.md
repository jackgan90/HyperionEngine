## Why

Import task/draft lifecycles and automation jobs use mutable strings for internal state. Runtime services and GUI consumers repeat protocol tokens, allowing typos or display changes to alter behavior.

## What Changes

- Define separate domain enums for import tasks, import drafts and automation jobs.
- Migrate lifecycle decisions and GUI consumers to typed states; convert explicitly at wire boundaries.
- Preserve current status tokens, reflected string schemas, operation IDs, polling, cancellation, pruning and shutdown behavior.
- Cover terminal failures, draft retry/editability and schema/round-trip compatibility.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-asset-import`: Require domain-owned typed import task and draft state with compatible wire projections.
- `engine-automation`: Require typed job lifecycle state and stable external status behavior.

## Impact

Runtime/AssetImport, Runtime/Automation, their Editor/automation consumers and focused tests. No shared global state machine, protocol revision, new import feature, cancellation policy change or Editor transition refactor.
