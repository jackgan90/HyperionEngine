# compute-pipelines Specification

## Purpose
Define reusable compute shader selection, reflected parameter binding, storage resources, dispatch and GPU lifetime guarantees across Renderer and RHI.
## Requirements
### Requirement: General compute programs and dispatch
The engine SHALL expose an independently selectable compute shader source, entry and defines, a compute pipeline, named reflected parameters and three-dimensional dispatch counts. Compute SHALL execute at declared graph positions on the supported queue without geometry or implicit attachments. Reflected group dimensions SHALL support ceiling-divided dispatch for a work extent.

#### Scenario: Independent compute workloads
- **WHEN** a client selects two different compute shaders and changes numeric/resource parameters between owned dispatch publications
- **THEN** each dispatch produces the expected independent result and previously published values remain unchanged

### Requirement: Compute resource bindings and validation
Compute SHALL support constant buffers, sampled textures, samplers, read-only structured/raw buffers, writable 2D texture mip views and writable structured/raw buffers. Capabilities and validation SHALL reject unsupported stage/kind/format, foreign resources, invalid counts, ranges, strides, missing slots and incompatible pipelines before dispatch. Graphics SHALL be able to sample declared compute outputs.

#### Scenario: Storage output consumed by another pass
- **WHEN** compute writes a texture mip or storage buffer and a later compute or graphics pass reads it
- **THEN** the consumer observes the completed bytes with compatible views and resource states

#### Scenario: Invalid binding
- **WHEN** a dispatch uses a graphics pipeline, foreign payload, wrong storage type or out-of-range view
- **THEN** validation fails without executing the invalid dispatch

### Requirement: Immutable compute resources and reuse
Renderer compute preparation SHALL own frozen parameters, perform native operations on RHI 0 and retain pipelines, descriptors, constant slices and storage generations through GPU completion. Stable shader/layout/view bindings SHALL be reusable across numeric changes. Failure and retirement SHALL preserve existing fence and cancellation guarantees.

#### Scenario: Numeric-only update
- **WHEN** a compute parameter changes while shader, layout and resource views remain identical
- **THEN** constants update without recreating the compatible pipeline or descriptor set

#### Scenario: Retire after submitted failure
- **WHEN** compute resources are replaced or released after work was submitted and presentation fails
- **THEN** those resources remain alive until completion and the next valid frame can execute
