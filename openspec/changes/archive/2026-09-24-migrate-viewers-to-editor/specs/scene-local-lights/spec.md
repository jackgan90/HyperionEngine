## MODIFIED Requirements

### Requirement: Editable diagnostic and Sponza acceptance
Editor SHALL expose creation/editing of both local light types, influence visualization, per-type visibility/draw statistics and algorithm availability. The default Sponza scene SHALL retain its authored point lights approximating the floor pools and nearby architectural lighting of the Khronos README Screenshot while preserving its calibrated camera and model/material assets. Default clustered rendering SHALL preserve the existing point-light appearance within documented numerical tolerance. Delivery SHALL include real D3D12 clustered/volume comparisons, documented visual limitations and validation/resource evidence.

#### Scenario: Default Sponza run
- **WHEN** the default Editor Sponza scene is rendered with Deferred
- **THEN** clustering is enabled, existing local illumination is preserved, lights survive save/reload, and GPU validation reports no errors

#### Scenario: Stable and moving workloads
- **WHEN** ready scenes render with static/moving cameras, light edits, resize and pipeline switching
- **THEN** resource use remains bounded and recorded statistics distinguish model work, cluster preparation and legacy light-volume work
