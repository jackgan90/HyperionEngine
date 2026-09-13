# scene-local-lights Specification

## Purpose
Define scene-owned point and spot lights with persistent typed data, extensible visibility queries and shared attenuated direct lighting. Specify default clustered shading across Deferred and HDR Forward, the optional Deferred light-volume fallback, SceneViewer authoring and Sponza visual acceptance.
## Requirements
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
Local lighting SHALL default to clustered evaluation in Deferred and HDR Forward, including lit transparency. When clustering is disabled, Deferred SHALL draw closed conservative sphere/cone volumes into HDR with additive RGB, preserved alpha, CullFront inside and outside, disabled Z clipping and no depth test/write/attachment. Volume shaders SHALL reconstruct the actual receiver and reject invalid or out-of-influence pixels. Each light SHALL contribute at most once per receiver sample. With clustering disabled, Forward and forward transparency SHALL receive no local contribution.

#### Scenario: Camera crosses the volume
- **WHEN** clustering is disabled and a camera moves outside, inside or across a sphere/cone and its near/far planes intersect the proxy
- **THEN** valid receivers remain illuminated once without brightness doubling or clipping holes in either depth convention

#### Scenario: Forward selection
- **WHEN** the user selects Forward with local lights present
- **THEN** clustered lighting illuminates lit models by default, while disabling clustering retains editable lights and disables local contribution

### Requirement: Editable diagnostic and Sponza acceptance
SceneViewer SHALL expose creation/editing of both local light types, influence visualization, per-type visibility/draw statistics and algorithm availability. The default Sponza scene SHALL retain its authored point lights approximating the floor pools and nearby architectural lighting of the Khronos README Screenshot while preserving its calibrated camera and model/material assets. Default clustered rendering SHALL preserve the existing point-light appearance within documented numerical tolerance. Delivery SHALL include real D3D12 clustered/volume comparisons, documented visual limitations and validation/resource evidence.

#### Scenario: Default Sponza run
- **WHEN** the default SceneViewer Sponza scene is rendered with Deferred
- **THEN** clustering is enabled, existing local illumination is preserved, lights survive save/reload, and GPU validation reports no errors

#### Scenario: Stable and moving workloads
- **WHEN** ready scenes render with static/moving cameras, light edits, resize and pipeline switching
- **THEN** resource use remains bounded and recorded statistics distinguish model work, cluster preparation and legacy light-volume work
