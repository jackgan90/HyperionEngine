# clustered-local-lighting Specification

## Purpose
Define conservative per-view cluster lists, immutable GPU resources and shared point/spot shading across Deferred and HDR Forward. Specify default enablement, legacy fallback and measurable visual and resource acceptance.
## Requirements
### Requirement: Conservative per-view cluster lists
Renderer SHALL build regular XY and logarithmic positive-view-depth clusters on CPU from immutable scene publications and existing light visibility queries. Each shading point SHALL compute its cluster index and read offset/count, light indices and attributes from separate structured buffers. Lists SHALL contain every potentially contributing light without duplicates or silent capacity truncation and SHALL support transparent receiver depths without a depth prepass.

#### Scenario: Boundaries and visibility
- **WHEN** point or spot bounds cross a tile, depth slice, near/far plane or camera position under standard/reversed Z or a sub-viewport
- **THEN** affected receivers retain the correct contribution and every lookup remains in bounds

#### Scenario: Dense and empty clusters
- **WHEN** a cluster has zero lights or more than a conventional fixed per-cell limit
- **THEN** lookup returns zero or the complete checked list, respectively, without discarding fragments

### Requirement: Shared cross-pipeline lighting
Clustered shading SHALL use the existing direct BRDF and point/spot attenuation. Deferred SHALL fuse local and directional light calculation in one fullscreen pass when effective directional radiance exists; otherwise it SHALL use a dedicated fullscreen cluster pass. Environment/emissive SHALL be applied once. HDR Forward opaque/masked and lit transparency in both pipelines SHALL evaluate their own cluster lists. Local lights SHALL NOT gain shadows or affect Unlit, shadow caster or legacy display routes.

#### Scenario: Three integration routes
- **WHEN** rendering Deferred with a directional light, Deferred without one, or HDR Forward
- **THEN** local lighting runs at the specified shading location with no volume draws and agrees on equivalent receivers

### Requirement: Immutable bounded cluster resources
Cluster descriptions SHALL match their scene publication and actual rendering view, reuse unchanged data, update assignment independently of radiance-only changes, and retain queued generations through normal GPU completion. Resource sets SHALL retire without permanent growth or GPU idle waits.

#### Scenario: Queued changes and sustained motion
- **WHEN** lights are edited/removed, cameras move, viewports resize or modes switch with frames queued
- **THEN** old frames retain their complete inputs, new frames use current inputs and resource allocations remain bounded

### Requirement: Default enablement and measurable acceptance
Cluster lighting SHALL default enabled, expose persisted settings/CLI/GUI control and report its actual algorithm and list statistics. Disabling SHALL stop cluster construction and restore previous Deferred volumes and Forward local-light exclusion. Delivery SHALL compare the unchanged current SceneViewer Sponza scene against the old rendering path and record real D3D12 validation, numerical image differences and stationary/moving workload measurements.

#### Scenario: Existing Sponza appearance
- **WHEN** loading the existing Sponza scene with default settings
- **THEN** its three authored point lights retain the previous appearance within documented tight numerical tolerance without retuning scene assets

#### Scenario: Toggle fallback
- **WHEN** clustering is disabled in either pipeline
- **THEN** rendering matches the previous version and no stale cluster contribution remains
