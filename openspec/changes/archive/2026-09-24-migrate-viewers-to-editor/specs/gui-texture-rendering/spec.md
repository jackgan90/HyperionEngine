## MODIFIED Requirements

### Requirement: Engine-owned GUI rendering
GUI rendering SHALL use a reusable Runtime renderer with buffers, textures, binding sets and pipelines created by Hyperion RHI. Public GUI contracts SHALL contain no ImGui or native graphics API types.

#### Scenario: Editor and editor share the renderer
- **WHEN** DebugUI or the Editor submits a GUI snapshot
- **THEN** both use the common engine renderer while preserving independent application UI behavior
