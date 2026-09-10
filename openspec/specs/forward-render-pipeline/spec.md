# forward-render-pipeline Specification

## Purpose
Define Renderer-owned shadow, forward and extension pass orchestration over a frozen scene view family with shared resources and independent per-view data and diagnostics.
## Requirements
### Requirement: Renderer-owned pass orchestration

A dedicated Renderer pipeline class SHALL express shadow depth, forward scene, extension passes and presentation through explicit graph attachment and read declarations. View camera/culling data SHALL be separate from pass target descriptions. Pipeline target compatibility SHALL derive from declared attachment signatures, not depth-target presence. Viewer SHALL supply frame inputs and invoke that pipeline without owning scene pass ordering.

#### Scenario: Shadowed frame with GUI
- **WHEN** SceneViewer renders with shadows and GUI enabled
- **THEN** explicit shadow writes precede forward reads and GUI loads scene color with unchanged overlay semantics

#### Scenario: Empty shadow view
- **WHEN** a shadow view contains no drawable casters
- **THEN** its declared depth attachment is still cleared and stored for valid forward sampling

### Requirement: Shared scene view family
Renderer SHALL prepare the main view and several shadow views from one frozen scene/frame boundary with unique stable identities, independent attachments and per-view statistics. Spatial maintenance SHALL occur once per family before read-only view queries.

#### Scenario: Five views
- **WHEN** a frame uses one main and four shadow views
- **THEN** they share geometry and material snapshots, use correct per-view matrices and visibility, and expose separate counts without overwriting main-view statistics
