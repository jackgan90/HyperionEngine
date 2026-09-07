## MODIFIED Requirements

### Requirement: Scene-wide pass organization
The Renderer SHALL aggregate scene items by view, target and required rendering state instead of allocating a graph pass per model or primitive. Scene pass organization SHALL initialize depth once before dependent scene depth use and preserve it for subsequent scene draws within the same view. Blended scene items sharing a view and target SHALL be ordered across model boundaries using the documented stable center-depth policy. Material-selected queues and effective state SHALL drive this organization. Compatible draws differing only in pipeline state SHALL remain in one pass; state changes SHALL not cause repeated clears. Different views SHALL have explicit viewport and depth initialization boundaries.

#### Scenario: Many primitives in one scene
- **WHEN** the primitive count exceeds the backend recording-context count but the aggregate pass count remains supported
- **THEN** the scene renders without consuming one recording context per primitive

#### Scenario: Shared scene depth
- **WHEN** independently registered models overlap in a depth-enabled scene
- **THEN** later model draws do not clear earlier scene depth and nearer opaque surfaces occlude farther surfaces

#### Scenario: Mixed depth settings
- **WHEN** compatible scene draws alternate depth-enabled and depth-disabled materials
- **THEN** their own pipeline states control depth behavior without requiring one graph pass per change

#### Scenario: Opaque and masked order preservation
- **WHEN** opaque and masked primitives are interleaved in stable submission order, including equal-depth surfaces
- **THEN** they share the opaque ordering bucket and preserve their original relative order through migration

#### Scenario: Multiple views in a frozen family
- **WHEN** BuildViews prepares multiple views in one frame family
- **THEN** it uses one Render control-message boundary, unique graph pass names and explicit per-view viewport/depth initialization, without mixing material revisions

## ADDED Requirements

### Requirement: Limited depth and stencil attachment validation
The existing single-color graph SHALL validate depth and stencil initialization separately for the configured D32 or D32S8 target. Stencil operations SHALL require a stencil-capable attachment, with initialized contents before load. This change SHALL NOT imply support for arbitrary offscreen resources, multiple color targets or resolve operations.

#### Scenario: Stencil unavailable
- **WHEN** a pass requires stencil while the configured depth target has no stencil component
- **THEN** validation fails before recording with the target incompatibility

#### Scenario: Undefined stencil load
- **WHEN** the first stencil-using pass attempts to load uncleared stencil contents
- **THEN** graph compilation fails; an explicit first clear followed by later loads is accepted
