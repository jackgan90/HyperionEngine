# scene-visibility Specification

## Purpose
Define conservative Render-thread visibility, replaceable spatial organization, BVH maintenance and comparable culling modes before primitive collection.
## Requirements
### Requirement: Render-owned spatial visibility
Render SHALL maintain geometry culling groups and a replaceable spatial index separately from logical node ownership and hierarchy. The initial index SHALL remain a binary AABB BVH supporting batched topology rebuild and transform refit. Camera and directional/environment-light nodes SHALL NOT be inserted as geometry primitives or unbounded geometry groups. Camera or light-only motion SHALL NOT rebuild or refit unchanged geometry spatial data. Ancestor changes SHALL update the effective bounds of only affected model descendants before visibility queries.

#### Scenario: Moving and removing a group
- **WHEN** a model node or its ancestor moves across a frustum plane, or the model is removed
- **THEN** subsequent queries use updated bounds and exclude removed generations

#### Scenario: Camera and light motion
- **WHEN** cameras and directional lights move while geometry remains unchanged
- **THEN** current views and shadow volumes query existing geometry data with no geometry index rebuild/refit caused solely by those changes

#### Scenario: Nongeometry scene nodes
- **WHEN** groups, cameras and lights are added to a scene
- **THEN** logical node counts increase without artificial geometry primitive/group counts or degraded unbounded geometry queries

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

### Requirement: Hierarchy propagation preserves visibility equivalence
Hierarchical world transforms and effective model enabled/visible state SHALL feed the existing conservative group/primitive/item filtering paths. BVH and linear modes SHALL produce equivalent visible content after ancestor edits, reparenting, deletion and asynchronous resource readiness. Each main or shadow view SHALL perform its own visibility query; logical scene parentage SHALL NOT substitute for spatial hierarchy.

#### Scenario: Mixed subtree motion
- **WHEN** an ancestor containing several models, a camera and a light moves
- **THEN** only affected model bounds update, camera/light pose resolves from the same publication, and BVH and linear rendering agree

#### Scenario: Offscreen shadow caster under a parent
- **WHEN** a parented model is outside the main view but casts onto a visible receiver
- **THEN** shadow visibility still includes it and parent movement updates its shadow without relying on main-view visible items
