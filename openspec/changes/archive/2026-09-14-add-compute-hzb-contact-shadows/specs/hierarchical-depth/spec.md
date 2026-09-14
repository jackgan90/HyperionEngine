## ADDED Requirements

### Requirement: Consumer-requested depth products
Renderer SHALL provide an HZB producer whose activation depends on explicit current graph/view consumer requests, independent of contact-shadow settings. Compatible requests SHALL share one product; incompatible source/view/depth/reduction identities SHALL remain distinct. No consumers SHALL mean no generation dispatches. Debug inspection SHALL be an explicit consumer.

#### Scenario: Consumers appear and disappear
- **WHEN** compatible consumers request HZB and then all stop requesting on a later frame
- **THEN** one product is generated while requested and generation ceases without a stale activation count

### Requirement: Precise hierarchical reduction
HZB SHALL use a float mip chain generated through compute. Nearest reduction SHALL use min in Standard-Z and max in Reversed-Z; farthest SHALL use the opposite operations. Source viewport/depth normalization and every valid source texel SHALL be covered, including odd, non-power-of-two and single-dimensional extents.

#### Scenario: Dual-convention numerical comparison
- **WHEN** known depth inputs are reduced in both conventions and both reduction modes
- **THEN** full-precision GPU results at every mip match the defined CPU reference including odd edges and far background

### Requirement: Current-view lifetime and visibility
HZB SHALL represent the declared current-frame depth generation and carry dimensions, viewport, convention, mip count and reduction semantics. Resize, camera/view changes, scene replacement and frame failure SHALL NOT expose a stale product as current.

#### Scenario: Resize while GPU work is pending
- **WHEN** a view resizes after submitting an HZB consumer
- **THEN** the new frame uses matching new descriptors and old resources remain valid for submitted users
