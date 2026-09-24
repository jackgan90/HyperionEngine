## Why

Supervised black-box MCP testing exposed gaps in content discovery, semantic schemas and host diagnostics that force agents to guess or read source despite working domain operations. Improving these shared contracts makes current and future operations easier to discover and use without rebuilding the automation framework.

## What Changes

- Verify and correct asset mutation metadata consistency with workspace queries.
- Register transitively referenced record types and add reusable enum/field semantics; provide numeric material editing through shared domain validation/history.
- Expose bounded mounted-content directory entries, including unrecognized native files and per-entry diagnostics, using existing filesystem services.
- Describe host-specific failed-document retention explicitly.
- Provide compact host health diagnostics and richer target identification with explicit, bounded liveness probing.
- Improve catalog search metadata and visible bootstrap constraints without aliases or semantic-search dependencies.
- Add contract/integration regressions and contributor documentation. Preserve existing IDs, wire values, transport layering and revision checks.

## Capabilities

### New Capabilities

- `automation-discoverability`: Self-describing schemas, semantic material values, content candidate discovery, concise health and identifiable application targets.

### Modified Capabilities

None. Existing operation contracts remain supported; additive requirements and consistency guarantees are specified in the new capability.

## Impact

Runtime Reflection, Automation, Content and AssetEditing; automation adapters and existing Editor/Viewer service providers; focused tests and automation documentation. No new platform provider, remote transport, plugin lifecycle or persistence format is introduced.
