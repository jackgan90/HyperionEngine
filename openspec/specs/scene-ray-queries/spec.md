# scene-ray-queries Specification

## Purpose
Define accelerated geometric ray queries over current Main-owned scene state, with shared immutable triangle data, incremental bounds updates, stable object handles and explicit query availability.

## Requirements
### Requirement: Main-owned scene queries
Scene SHALL expose nearest geometric ray queries over current Main state without reading Render-owned objects or consuming the render synchronization change stream. Results SHALL distinguish Hit, Miss and Unavailable and carry a generation-checked scene handle.

#### Scenario: Render acknowledgment precedes query
- **WHEN** a transform is committed and Renderer acknowledges the scene change before the next query
- **THEN** picking uses the new transform through independently invalidated query state

#### Scenario: Removed slot is reused
- **WHEN** a selected object is removed and another object reuses its slot
- **THEN** stale handles do not resolve to the new object

### Requirement: Incremental and shared acceleration
Scene SHALL maintain an object bounds index with transform refits and membership rebuilds. Triangle acceleration SHALL use compact immutable data shared by instances and be prepared off Main when opted in through scene loading. Unrelated metadata, camera and material edits SHALL NOT rebuild triangles.

#### Scenario: Repeated query without edits
- **WHEN** the same scene is queried repeatedly without geometry or transform changes
- **THEN** neither scene bounds nor triangle acceleration is rebuilt

#### Scenario: Optional preparation and shutdown
- **WHEN** a scene instance without picking enabled loads, or an enabled instance closes during preparation
- **THEN** the former does not build triangle acceleration and the latter cancels and joins preparation before releasing captured state

### Requirement: Exact static geometry filtering
Queries SHALL select the nearest accepted triangle within the world ray interval, respecting effective visibility, source selectors, section visibility and configured material culling. They SHALL handle affine transforms and deterministic equal-distance ties without treating an AABB intersection as a final hit.

#### Scenario: Bounds overlap without triangle hit
- **WHEN** the nearest candidate bounds contains no accepted triangle along the ray
- **THEN** traversal continues to a farther object's accepted triangle

#### Scenario: Equal-distance hits round down
- **WHEN** an accepted hit's reported float distance rounds below the intersection distance
- **THEN** traversal retains equal-distance candidates for stable tie comparison without extending the caller's near/far interval

#### Scenario: Material pass is excluded from the active view
- **WHEN** a material has a requested pass but matches that pass's configured exclusions
- **THEN** the query skips that pass and considers only eligible scheduled passes

#### Scenario: Nonuniform mirrored or singular transform
- **WHEN** geometry is transformed by nonuniform scale, reflection, shear or a singular affine matrix
- **THEN** any accepted hit reports the correct world distance and winding policy without invalid inverse use

#### Scenario: Partially loaded scene
- **WHEN** some geometry is prepared and other potentially intersecting geometry is unavailable
- **THEN** ready geometry remains queryable and uncertainty is reported without claiming a confirmed empty-space miss
