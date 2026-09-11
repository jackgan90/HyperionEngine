# depth-conventions Specification

## Purpose
Define startup-selected reversed-Z and standard-Z behavior across application configuration, projections, material state, render targets, caches, clip-space geometry and cascaded shadows.
## Requirements
### Requirement: Startup depth configuration
Applications SHALL default reversed_z to true and freeze the active depth convention at startup. Configuration edits SHALL be saveable for the next launch without changing active rendering.

#### Scenario: Missing setting
- **WHEN** an existing configuration omits reversed_z
- **THEN** ModelViewer, SceneViewer and Triangle use reversed-Z

#### Scenario: Standard fallback and pending edits
- **WHEN** reversed_z is false at startup and later edited to true
- **THEN** all submitted frames remain standard-Z and saving persists true for the next launch

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
