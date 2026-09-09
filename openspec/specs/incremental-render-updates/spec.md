# incremental-render-updates Specification

## Purpose
Define incremental Renderer preparation for shared engine parameters, stable scene items, independent immutable instance records and blocks, bounded view history, and verified performance delivery that preserves rendering and GPU lifetime contracts.

## Requirements
### Requirement: Shared engine changes preserve local material preparation
Renderer SHALL propagate compatible shared engine-only parameter changes without republishing a complete local material result for every stable object. Actual draw preparation SHALL observe the current shared and local values. Mixed dependencies, overrides, missing required inputs and resource transitions SHALL retain their existing semantics.

#### Scenario: Camera changes with fixed visibility
- **WHEN** a ready scene with unchanged objects/materials renders a new camera using separable View constants
- **THEN** shared View updates are prepared per compatible group, local material results and instance data are reused, and only changed constant blocks are uploaded

#### Scenario: Mixed or instance-dependent shared parameter
- **WHEN** a provider depends on both View and Object, or a changing shared value appears in an instance record
- **THEN** the affected values and instance data are refreshed correctly while unrelated stable data remains eligible for reuse

#### Scenario: Frozen earlier frame
- **WHEN** another view or later frame changes shared inputs while an earlier graph or GPU submission is retained
- **THEN** each submission continues to observe its own original effective values and published slices

### Requirement: View changes retain independent scene and batch data
Stable primitive preparation SHALL remain reusable across visibility/order changes. Independent sequential view families SHALL retain bounded reusable state without evicting each other solely because their view IDs differ. Custom collection and transparent ordering SHALL remain correct.

#### Scenario: Changing visible membership
- **WHEN** camera motion changes which stable mesh items are visible
- **THEN** the visible ordered set is correct and unchanged local preparation and compatible instance records are reused

#### Scenario: Two independent families
- **WHEN** one session repeatedly builds two different view families in the same frame
- **THEN** each family can reuse its stable preparation without cross-family values or unbounded retention

### Requirement: Independent instance blocks reuse immutable storage
Instance preparation SHALL cache reusable packed records and blocks using complete reflected layout and effective value compatibility. Unchanged independent blocks SHALL retain their immutable GPU slices when another block changes. Compatible block reuse SHALL not depend on unrelated view or pass identity. Histories SHALL be bounded and retired safely.

#### Scenario: One transformed object
- **WHEN** a transform changes while the same batch retains its member order and surface values
- **THEN** affected object data changes while the unchanged surface block is neither repacked nor uploaded

#### Scenario: Compatible views
- **WHEN** two views require the same instance block layout and ordered effective values
- **THEN** they can reuse the same packed block and published GPU slice

### Requirement: Performance delivery preserves rendering and measurement integrity
The change SHALL provide frozen-binary Debug/Release A/B measurements with readiness, draw coverage, validation, warmup, settings and distributions recorded. Static, small-motion, large-motion and fixed-visibility workloads SHALL be distinguished. Implementation documents SHALL describe the final code and report ineffective attempts or remaining bottlenecks. Existing GPU ownership and native validation SHALL remain enabled.

#### Scenario: Final delivery
- **WHEN** implementation is declared complete
- **THEN** current-source checks and regression tests pass, images and source-item coverage remain equivalent, performance results identify the exact binaries, and design/tasks/evidence match the delivered implementation
