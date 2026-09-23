## Why

Hyperion has reusable data reflection but no supported, discoverable invocation contract for agents. Editor-owned document behavior makes separate CLI/MCP implementations likely to drift; the foundation must prioritize maintainability, then identical human/agent semantics, ahead of adapting every existing feature.

## What Changes

- Add a protocol-independent, typed operation catalog with reflected request/result schemas, usage metadata, bounded discovery, structured errors, Main-owned invocation, and asynchronous jobs.
- Extend reflection with a strict natural-JSON projection and machine-readable shape information without changing native asset persistence.
- Extract the existing asset document/history/save implementation into a CPU-only Runtime module shared by the Editor and automation. Use asset inspection, rename, texture encoding, undo/redo and save/reopen as the first vertical slice.
- Add an opt-in, headless automation application with one-shot CLI, persistent JSON-lines sessions and an MCP stdio adapter over the same endpoint. No default network listener or runtime plugin loading.
- Publish extension rules, runnable examples, an explicit coverage inventory and regression checks so future features use shared domain services and one operation registration.
- Stage scene-document extraction, live Editor IPC attachment, renderer/RHI resource adapters, and broader feature coverage as follow-up adaptations; do not create alternative implementations for them in this change.

## Capabilities

### New Capabilities

- `engine-automation`: Operation registration, discovery, schemas, invocation, jobs and CLI/MCP adapters.
- `shared-asset-documents`: CPU-only document editing and history shared by GUI and automation.

### Modified Capabilities

None. Existing GUI interaction, serialized asset formats, plugin selection and executable names retain their contracts.

## Impact

- Reflection receives reusable schema/wire support; new Automation and AssetEditing runtime modules own protocol-independent contracts.
- New automation plugin/application compose CPU services through the existing plugin lifecycle; Runtime/Application remains minimal.
- Editor asset-document consumers link the shared implementation; native asset and texture services are reused.
- Tests cover type registration, wire validation, operation/job lifecycle, protocol behavior, shared document semantics, and unavailable/disabled providers. Documentation adds a required extension workflow and a coverage backlog.
