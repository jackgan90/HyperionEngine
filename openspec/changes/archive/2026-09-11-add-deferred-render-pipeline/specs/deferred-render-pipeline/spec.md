## ADDED Requirements

### Requirement: Traditional default Deferred pipeline
Renderer SHALL provide a default raster Deferred pipeline with opaque/masked GBuffer BasePass followed by a vertex/pixel fullscreen LightingPass producing HDR scene color. It SHALL share CSM visibility, material evaluation and lighting formulas with selectable Forward. Compute lighting and mobile single-pass deferred SHALL NOT be required.

#### Scenario: Default scene frame
- **WHEN** SceneViewer or ModelViewer runs without an explicit pipeline override
- **THEN** BasePass writes GBuffer/depth, Lighting reads them and CSM, and shared output presents the scene with zero validation errors

#### Scenario: Empty or masked scene
- **WHEN** no valid opaque pixels remain or a masked pixel is clipped
- **THEN** cleared GBuffer validity and depth prevent undefined shading and preserve the background or visible geometry behind the mask

### Requirement: Validated configurable GBuffer
GBuffer storage SHALL be configured through an engine-owned layout with ordered formats, semantic packing, clear values and versioned identity shared by resource allocation, shader variants and complete target/cache signatures. At least two precision configurations SHALL render successfully. Unsupported configurations SHALL fail explicitly before recording.

#### Scenario: Precision or size changes
- **WHEN** a configured layout or viewport size changes
- **THEN** matching resources and pipelines are selected and prior in-flight resources remain valid until GPU completion

#### Scenario: Invalid format
- **WHEN** a format cannot represent a required channel or cannot be rendered and sampled on the device
- **THEN** the pipeline reports an actionable incompatibility without reusing a stale target or shader

### Requirement: Compatible materials and shadow receivers
Opaque/masked DefaultLit SHALL initialize shared material parameters and encode them once. Transparent and Unlit materials SHALL retain supported behavior through explicit forward compatibility paths using common output. Routing SHALL prevent duplicated contributions. Position reconstruction SHALL honor viewport/depth conventions; shadow receiver evaluation SHALL preserve the pre-normal-map normal and handle depth discontinuities safely. Multi-model IDs SHALL remain separate from material-instance identity.

#### Scenario: Mixed materials
- **WHEN** opaque, masked, blended, Unlit, mirrored and instanced geometry overlap
- **THEN** depth, alpha clipping, global transparency order and exclusive routing remain correct

#### Scenario: Camera crosses a shadow silhouette
- **WHEN** reconstructed neighboring positions belong to different surfaces
- **THEN** shadow receiver correction remains bounded and does not use an unchecked cross-surface gradient

### Requirement: Reproducible deferred validation and performance
Delivery SHALL include correctness evidence and a CPU/GPU report comparing common-output Forward and Deferred on ready scenes, with build/workload identity, resolution/layout, static/moving cameras, CSM settings, repeated measurements, per-pass times and resource costs. GPU samples SHALL be associated with completed submission identities, and missing samples or regressions SHALL be reported.

#### Scenario: Performance comparison
- **WHEN** repeated Forward/Deferred runs are measured
- **THEN** the report identifies matching workloads, median/P95 frame times, GPU pass costs, validation status and limits without presenting estimates as measurements
