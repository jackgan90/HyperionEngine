## ADDED Requirements

### Requirement: Renderer-owned pass orchestration
A dedicated Renderer pipeline class SHALL express the frame sequence shadow depth, forward scene, extension passes and presentation. Viewer SHALL supply frame inputs and invoke that pipeline without owning scene pass ordering.

#### Scenario: Shadowed frame with GUI
- **WHEN** SceneViewer renders with shadows and GUI enabled
- **THEN** all shadow writes precede forward reads and GUI follows scene rendering with unchanged overlay semantics

### Requirement: Shared scene view family
Renderer SHALL prepare the main view and several shadow views from one frozen scene/frame boundary with unique stable identities, independent attachments and per-view statistics. Spatial maintenance SHALL occur once per family before read-only view queries.

#### Scenario: Five views
- **WHEN** a frame uses one main and four shadow views
- **THEN** they share geometry and material snapshots, use correct per-view matrices and visibility, and expose separate counts without overwriting main-view statistics
