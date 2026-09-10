## ADDED Requirements

### Requirement: Explicit resolved graphics attachments

RHI graphics commands SHALL contain explicit resolved attachment references, view formats and load/store operations. The backend SHALL bind only the declared targets and validate their compatibility with draw pipelines. Backbuffer and frame depth references SHALL be explicit even when they resolve to existing swapchain storage. Graph transitions SHALL identify their target resource, including presentation transitions. Existing recording concurrency, GPU-safe lifetime and failure recovery SHALL remain intact.

#### Scenario: Depth-only recording
- **WHEN** commands declare only a sampled D32 depth attachment
- **THEN** the backend binds that DSV with zero RTVs using its dimensions and performs the declared depth load/store operations

#### Scenario: Forward and overlay
- **WHEN** forward commands declare backbuffer and main depth and GUI declares only backbuffer Load/Store
- **THEN** forward depth and color are initialized as declared and the GUI overlays without clearing prior scene color
