## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Hierarchy propagation preserves visibility equivalence
Hierarchical world transforms and effective model enabled/visible state SHALL feed the existing conservative group/primitive/item filtering paths. BVH and linear modes SHALL produce equivalent visible content after ancestor edits, reparenting, deletion and asynchronous resource readiness. Each main or shadow view SHALL perform its own visibility query; logical scene parentage SHALL NOT substitute for spatial hierarchy.

#### Scenario: Mixed subtree motion
- **WHEN** an ancestor containing several models, a camera and a light moves
- **THEN** only affected model bounds update, camera/light pose resolves from the same publication, and BVH and linear rendering agree

#### Scenario: Offscreen shadow caster under a parent
- **WHEN** a parented model is outside the main view but casts onto a visible receiver
- **THEN** shadow visibility still includes it and parent movement updates its shadow without relying on main-view visible items
