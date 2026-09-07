# scene-visibility Specification

## Purpose
Define conservative Render-thread visibility, replaceable spatial organization, BVH maintenance and comparable culling modes before primitive collection.

## Requirements
### Requirement: Render-owned spatial visibility
Render SHALL maintain culling groups and a replaceable spatial index separately from logical ownership. The initial index SHALL be a binary AABB BVH supporting batched topology rebuild and transform refit. Camera motion SHALL NOT rebuild unchanged spatial data.

#### Scenario: Moving and removing a group
- **WHEN** an instance moves across a frustum plane or is removed
- **THEN** subsequent queries use updated bounds and exclude removed generations

### Requirement: Conservative pre-collection filtering
BVH group queries and primitive bounds tests SHALL run before Collect on Render. Unknown, nonfinite, intersecting or clip-space bounds SHALL NOT justify world-space rejection. Group bounds SHALL include every member. Visibility SHALL be view-local and preserve deterministic collection order.

#### Scenario: Unknown multi-item primitive
- **WHEN** a primitive cannot bound its multiple possible outputs
- **THEN** it remains eligible for collection without truncating its output

#### Scenario: Upload completes outside the camera
- **WHEN** a pending resource becomes ready while its group is outside the view
- **THEN** bounds readiness progresses and moving the camera later reveals the object correctly

#### Scenario: Mirrored or boundary transform
- **WHEN** bounds are mirrored, nonuniformly scaled or intersect the near plane
- **THEN** conservative visibility does not omit potentially visible geometry

### Requirement: Comparable visibility modes
Renderer SHALL expose disabled, linear frustum and BVH frustum modes, independent counters for groups, traversal, candidates, collection and items, and index maintenance/query timings. BVH and linear modes SHALL agree under the same leaf test and preserve existing draw organization.

#### Scenario: Mostly invisible population
- **WHEN** a fixed view sees a small subset of many separated groups
- **THEN** BVH traverses fewer group tests than linear mode and both collect the same primitives
