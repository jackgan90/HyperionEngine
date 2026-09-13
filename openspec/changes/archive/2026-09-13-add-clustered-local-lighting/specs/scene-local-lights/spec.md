## MODIFIED Requirements

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
