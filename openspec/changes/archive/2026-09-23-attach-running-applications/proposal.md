## Why

Automation currently owns an independent headless workspace and cannot operate on a scene already open in Editor or Scene Viewer. Live attachment needs shared document authority and a transport boundary that can later support other desktop platforms and remote devices without duplicating engine operations.

## What Changes

- Add a platform-neutral, asynchronous transport contract and registry, with Windows current-user named pipes as the first production provider and an in-memory contract-test provider.
- Add target discovery, explicit connection routing, bounded framing, version/identity handshake, and application-owned automation sessions. Preserve existing standalone CLI/JSONL/MCP use; add target management and `--attach`.
- Enable local attachment by default in Editor and Viewer, with explicit plugin disablement. Keep transport and session lifecycle inside plugins and execute requests at Main update boundaries.
- Extract shared CPU scene-document editing/history/save-state services and bind GUI and automation to the same instances. Expose current-scene discovery, object queries, transform editing, Editor undo/redo, and save with host-specific capabilities.
- Keep remote networking, other operating-system providers, reconnect/resume, full asset-workspace attachment, and comprehensive API coverage explicitly deferred.
- Update architecture, usage, extension and coverage documentation; validate real CLI/MCP attachment and GUI-equivalent state/history/persistence.

## Capabilities

### New Capabilities
- `automation-transport`: Replaceable stream transports, discovery, identity/handshake, bounded messaging and connection lifecycle.
- `live-application-automation`: Default local application attachment, target routing and shared live scene operations.
- `shared-scene-documents`: UI-independent document transactions, history and save-state authority shared by GUI and automation.

### Modified Capabilities
- `engine-automation`: Explicit target routing and target-owned discovery/invocation through existing frontends.

## Impact

Adds Runtime/Transport and Runtime/SceneEditing; extends Runtime/Automation, Renderer scene editing integration, Plugins/Automation, Editor and SceneViewer, and application composition. Windows APIs remain private; public communication contracts do not depend on native handles, local process identifiers, GUI, Renderer or MCP. Existing operation IDs and standalone defaults remain compatible. No network listener, content-mount configuration, automatic mutation retry, Git commit, or OpenSpec archive is included.
