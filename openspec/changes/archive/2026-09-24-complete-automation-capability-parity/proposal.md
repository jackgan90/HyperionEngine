## Why

The automation catalog, jobs, reflection wire schema and live application attachment are established, but the current adapters expose only a small subset of existing human workflows. Agents cannot yet complete scene authoring, edit the live asset workspace or control views through the same services as the GUI. Closing these gaps now provides a maintainable baseline for future features.

## What Changes

- Inventory existing Editor, Scene Viewer and AssetTool tasks and track their automation counterparts and parity evidence.
- Extend scene document operations with selection, node creation/deletion/hierarchy, reflected components, settings and asset references, preserving atomic validation and shared history.
- Extract focused UI-independent entry points for document transitions, live content-root changes and the actual Editor asset workspace; share asset property/reference editing and persistence with standalone automation.
- Expose existing view/camera, rendering options, preview and capture/result retrieval capabilities using typed host services and existing jobs.
- Adapt existing asset import/publication workflows and content discovery without launching test helpers.
- Keep fixed CLI/MCP bootstrap methods, connection/transport/session architecture and explicit typed registration. Add only small domain contracts and service extractions; no framework-level redesign, remote transport, plugin hot loading or arbitrary C++ RPC.
- Update development documentation so new user-facing features include shared service, registered operation and task-equivalence validation.

## Capabilities

### New Capabilities

- `automation-capability-parity`: Discoverable, typed equivalents of existing human tasks across scene authoring, live asset workspaces, content/import workflows and host views/results, with explicit host availability and parity evidence.

### Modified Capabilities

None. Existing automation, document, content and attachment contracts remain prerequisites; this change adds their domain adapters and bounded shared service entry points.

## Impact

- Runtime SceneEditing, AssetEditing and related domain services; reflected request/result records.
- Automation, Editor, SceneViewer and existing rendering/capture/import providers; application plugin composition where required.
- CPU domain/adapter tests and real CLI/MCP attached-application acceptance, including GUI history/persistence equivalence and lifecycle/absence paths.
- Automation, Editor, connection and feature development documentation. No commit or archive is part of this implementation request.
