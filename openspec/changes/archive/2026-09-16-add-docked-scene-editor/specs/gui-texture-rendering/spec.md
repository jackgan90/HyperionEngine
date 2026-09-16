## ADDED Requirements

### Requirement: Preserve legacy material output views
RGBA8 offscreen scene output SHALL preserve each legacy Forward material's linear or sRGB view selection, including mixed batches in one scene.

#### Scenario: Mixed legacy output
- **WHEN** a scene with linear and sRGB legacy Forward materials is rendered to the editor viewport texture
- **THEN** rendering completes and the material colors match their corresponding backbuffer output

### Requirement: Engine-owned GUI rendering
GUI rendering SHALL use a reusable Runtime renderer with buffers, textures, binding sets and pipelines created by Hyperion RHI. Public GUI contracts SHALL contain no ImGui or native graphics API types.

#### Scenario: Viewer and editor share the renderer
- **WHEN** DebugUI or the Editor submits a GUI snapshot
- **THEN** both use the common engine renderer while preserving independent application UI behavior

### Requirement: Texture-aware immutable draw snapshots
GUI draw commands SHALL preserve opaque texture IDs and offsets. Frame texture bindings SHALL retain the source and lifetime needed by deferred preparation, declare GPU-produced texture reads and reject unresolved IDs.

#### Scenario: Scene image and font in one frame
- **WHEN** text and a scene image are submitted together
- **THEN** each command uses its own texture binding with correct clipping and color encoding

#### Scenario: Unknown image binding
- **WHEN** a GUI command references an unregistered texture ID
- **THEN** the renderer reports the missing binding rather than drawing with the font texture
