## ADDED Requirements

### Requirement: Distinct persistent local light nodes
Scene SHALL own distinct point and spot light payloads with stable node identity, hierarchy, effective enablement, linear color, intensity and positive world-unit range. Spot lights SHALL additionally own validated inner/outer half angles and derive emission from world minus-Z. Node scale SHALL NOT rescale authored range. Source/native serialization and runtime save SHALL preserve both types and migrate old scenes without adding lights.

#### Scenario: Edit save and reload
- **WHEN** both light types are edited, reparented, disabled and saved
- **THEN** reload preserves types, parameters, IDs, local transforms and enabled state

#### Scenario: Invalid light parameters
- **WHEN** a light has nonfinite/negative radiance, invalid range, or unordered/degenerate cone angles
- **THEN** the operation fails without changing scene state or revision

### Requirement: Coherent light publication and conservative visibility
Renderer SHALL publish local lights with the same scene token as camera/geometry and query per-view light bounds through ISceneVisibility and the existing spatial index contract. None, Linear and Bvh SHALL preserve visible results; light geometry SHALL NOT become a model or shadow caster. Parameter-only updates SHALL NOT rebuild unchanged spatial topology or GPU geometry.

#### Scenario: Offscreen light position
- **WHEN** a light position is outside the camera frustum but its influence intersects visible receivers
- **THEN** conservative bounds retain its contribution

#### Scenario: Queued frames and removal
- **WHEN** lights change or are removed after a frame is queued
- **THEN** queued data remains immutable and later frames use the new publication without stale contributions

### Requirement: Shared attenuated direct lighting
Point, spot and directional lights SHALL use the same direct BRDF. Local lights SHALL use smooth finite inverse-square attenuation, spots SHALL additionally fade between their inner/outer cones, and ambient/emissive terms SHALL be added only once. Light-center evaluation SHALL remain finite.

#### Scenario: Overlapping lights
- **WHEN** two lights overlap a valid opaque receiver
- **THEN** its linear HDR direct contribution equals the sum of the independent contributions within numerical tolerance

### Requirement: Deferred light volumes with stable coverage
Deferred SHALL draw closed conservative sphere/cone volumes into HDR with additive RGB, preserved alpha, CullFront inside and outside, disabled Z clipping and no depth test/write/attachment. Shaders SHALL reconstruct the actual receiver and reject invalid or out-of-influence pixels. Each light SHALL contribute at most once per receiver sample. Forward and forward transparency SHALL receive no local-light list or local contribution.

#### Scenario: Camera crosses the volume
- **WHEN** a camera moves outside, inside or across a sphere/cone and its near/far planes intersect the proxy
- **THEN** valid receivers remain illuminated once without brightness doubling or clipping holes in either depth convention

#### Scenario: Forward selection
- **WHEN** the user selects Forward with local lights present
- **THEN** the scene retains editable lights, the algorithm reports inactive, and no local draws or shader list bindings occur

### Requirement: Editable diagnostic and Sponza acceptance
SceneViewer SHALL expose creation/editing of both local light types, influence visualization, per-type visibility/draw statistics and algorithm availability. The default Sponza scene SHALL contain authored point lights approximating the floor pools and nearby architectural lighting of the Khronos README Screenshot while preserving its calibrated camera and model/material assets. Delivery SHALL include real D3D12 on/off captures, a reference comparison, documented visual limitations and validation/resource evidence.

#### Scenario: Default Sponza run
- **WHEN** the default SceneViewer Sponza scene is rendered with Deferred
- **THEN** distinct soft local floor illumination and nearby surface lighting approach the reference, lights survive save/reload, and GPU validation reports no errors

#### Scenario: Stable and moving workloads
- **WHEN** ready scenes render with static/moving cameras, light edits, resize and pipeline switching
- **THEN** resource use remains bounded and the recorded statistics distinguish model work from local light work
