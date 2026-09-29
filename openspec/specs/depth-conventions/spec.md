# depth-conventions Specification

## Purpose
Define reversed-Z and standard-Z behavior across startup configuration, live Editor switching, projections, material state, render targets, caches, clip-space geometry and cascaded shadows.
## Requirements

### Requirement: Startup depth configuration
Editor SHALL default reversedZ to true, initialize it from saved settings, and apply validated changes through its shared GUI/automation settings service to the next rendered frame of scene and three-dimensional asset viewports. Saving SHALL remain an explicit independent operation. Changes SHALL NOT mutate scene or asset history.

#### Scenario: Missing setting
- **WHEN** an existing configuration omits reversedZ
- **THEN** Editor and its scene/asset viewports use reversed-Z

#### Scenario: Standard fallback and live edits
- **WHEN** reversedZ is false at startup and later edited to true
- **THEN** the next rendered scene and 3D asset preview frames use reversed-Z without restarting, reopening documents or recreating the swapchain
- **AND** saving persists true for the next launch

#### Scenario: Hidden viewport resumes
- **WHEN** depth changes while a viewport is hidden or minimized or before an asset preview is opened
- **THEN** its next rendered frame uses the latest committed depth convention

### Requirement: Consistent depth mapping
Reversed views SHALL map physical near to one and far to zero, clear depth to zero and use GreaterEqual for default depth-tested built-in passes. Standard views SHALL map near to zero and far to one, clear to one and use LessEqual. Both SHALL retain ordered physical clipping distances and zero-to-one clipping.

#### Scenario: Projection and target agreement
- **WHEN** a view renders overlapping geometry in either convention
- **THEN** the closest fragment wins and near/far clipping matches the physical camera interval

### Requirement: Material and preparation compatibility
Built-in scene passes SHALL use view-relative comparison and depth bias. Explicit raw material state and stencil comparisons SHALL retain their specified meaning. Ordinary and instanced paths SHALL resolve the same effective state, and caches SHALL distinguish incompatible conventions.

#### Scenario: Raw state escape hatch
- **WHEN** a custom material explicitly uses raw Less in a reversed view
- **THEN** its effective comparison remains Less

#### Scenario: Ordinary and instanced rendering
- **WHEN** the same supported material renders in ordinary and instanced form
- **THEN** depth visibility is equivalent in both conventions

### Requirement: Depth consumers
Forward, Deferred and compatibility passes SHALL produce equivalent visible geometry under either convention. Transparency SHALL remain back-to-front, and Deferred world-position reconstruction SHALL use the matching projection. UI without depth SHALL retain its behavior.

#### Scenario: Transparent overlapping surfaces
- **WHEN** multiple blended surfaces overlap at distinct depths
- **THEN** both depth conventions produce the same compositing order

### Requirement: Cascaded shadows
CSM SHALL follow the selected convention consistently in projection, targets, comparison sampling, neutral textures and receiver/caster bias. It SHALL retain independent caster visibility and correct lit/occluded behavior.

#### Scenario: Shadow receiver
- **WHEN** an opaque or masked caster shadows a visible receiver in either convention
- **THEN** occluded and lit regions remain correct in Forward and Deferred

### Requirement: Triangle depth participation
Triangle SHALL enable depth testing and writing and map its clip-space depth to the active convention without changing screen-space shape.

#### Scenario: Triangle startup
- **WHEN** Triangle starts with either configuration
- **THEN** it remains visible and its draws use the corresponding depth-tested state

### Requirement: Validation evidence
The change SHALL include CPU depth mapping/clipping/sorting tests, GPU depth/material/shadow coverage, both-mode application checks and repository style/build verification.

#### Scenario: Delivery
- **WHEN** implementation is declared complete
- **THEN** recorded validation identifies configurations exercised and any remaining limitations

### Requirement: Clip-space geometry retains geometric orientation
For bClipSpace primitives, World SHALL describe geometry in canonical Standard depth. Renderer SHALL adapt WVP, culling and depth sorting to the view convention without changing geometric facing, normals or orientation.

#### Scenario: Culled and mirrored clip geometry
- **WHEN** ordinary or instanced clip-space geometry renders with either depth convention, including a geometric mirror
- **THEN** front/back classification and culling remain equivalent and the closest surface wins

#### Scenario: Identical matrices with different conventions
- **WHEN** a clip-space view changes only its depth convention in a low-level rendering session
- **THEN** material preparation does not reuse the prior convention's WVP

### Requirement: Live depth consistency
Editor SHALL use one committed convention for scene camera projections, depth targets and comparisons, shadows, HZB/contact shadows, transparency, sky, picking, placement and debug drawing. Frozen culling SHALL retain its physical frustum and frozen state across a depth change. Old GPU resources SHALL remain valid until in-flight use completes.

#### Scenario: Alternating frames
- **WHEN** the same scene session and swapchain render repeated Standard/Reversed switches
- **THEN** Forward and Deferred preserve visible geometry, shading and compositing within rendering tolerance, with no validation errors or unbounded retained resource growth

#### Scenario: Frozen culling and interactions
- **WHEN** a user freezes culling, moves the camera and changes depth convention
- **THEN** the frozen frustum retains its original physical coverage and picking/placement use the current camera with the committed convention

### Requirement: Live depth settings contract
The render.settings.get and render.settings.set operations SHALL expose version 2 metadata and preserve existing IDs and field shapes. activeReversedZ SHALL report the convention committed for subsequent frames. Set completion SHALL mean Main settings committed, not GPU presentation. Save and persistent record format SHALL remain version 1. GUI and automation SHALL share validation, revision and persistence behavior.

#### Scenario: Discovery and mutation
- **WHEN** a client describes and invokes render.settings.set with the current revision and valid values
- **THEN** the description explains next-frame application, the response and subsequent get report matching requested/active values and a new revision, and scene/asset history remains unchanged

#### Scenario: Rejected candidate
- **WHEN** a client submits invalid values or a stale revision
- **THEN** settings, active depth, frozen culling, saved files and revision remain unchanged

#### Scenario: Explicit persistence
- **WHEN** a user switches depth without saving and later explicitly saves
- **THEN** switching affects live rendering immediately and only saving updates the settings file used on restart
