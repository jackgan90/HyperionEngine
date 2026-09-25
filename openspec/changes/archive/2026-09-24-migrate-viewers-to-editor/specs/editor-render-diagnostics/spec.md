## ADDED Requirements

### Requirement: Shared render and visibility controls
Editor SHALL expose validated pipeline, GBuffer, clustered lighting, contact shadows, cascaded shadows, culling, frozen culling, batching, model bounds and light influence controls using UI-independent services shared with Automation. Debug state SHALL remain separate from scene history. Existing Editor defaults SHALL remain unchanged.

#### Scenario: Change visibility diagnostics
- **WHEN** a user or agent changes culling or batching and freezes the culling view
- **THEN** subsequent rendering uses those settings while display navigation and shadow views continue using the current camera

#### Scenario: Debug overlay isolation
- **WHEN** bounds or influence visualization is enabled in a resized docked viewport
- **THEN** geometry is projected and clipped to that viewport and does not change saved scene data

### Requirement: Shared diagnostic and capture workflow
Editor SHALL expose render statistics, profiling controls, completed PNG output and optional RenderDoc capture. Build/provider absence SHALL be discoverable and fail requested actions without affecting unrelated capabilities. Benchmark workloads and samples SHALL retain explicit readiness and frame identity.

#### Scenario: Optional provider absent
- **WHEN** capture or profiling is requested without its available provider
- **THEN** discovery and invocation report the limitation and ordinary rendering remains usable

### Requirement: Complete scene settings UI
Editor SHALL expose default camera, main directional light, environment light and KeepLocal/KeepWorld reparenting through the shared document. Persistent edits SHALL support history and save/reopen; browsing camera changes SHALL remain transient.

#### Scenario: Author environment selection
- **WHEN** the environment selection is changed, undone, redone and saved
- **THEN** GUI and Automation observe the same settings and reopening restores the saved selection
