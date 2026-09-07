## ADDED Requirements

### Requirement: Generic primitive material state
Primitive state SHALL reference immutable generic material snapshots and typed local overrides independently of geometry/section identity. Accepted related material updates SHALL obey existing atomic frame-boundary revision/generation rules. Neither Render collection nor RHI execution SHALL dereference mutable material instances owned by Main.

#### Scenario: Multiple primitives share a material edit
- **WHEN** Main publishes one material revision to a related batch of primitives
- **THEN** collection observes either the prior complete batch or the new complete batch and previous frame snapshots remain valid

#### Scenario: Stale material result
- **WHEN** preparation completes for a material revision superseded by a newer update or removed generation
- **THEN** it cannot replace the current selected material or revive the primitive

### Requirement: Material-aware conservative geometry bounds
Primitive coordinate space SHALL be explicit and independent of a model-specific material flag. Clip-space or vertex-deformed geometry SHALL not be rejected by stale world-space bounds. If a selected usage can move vertices outside known bounds, all pre-collection/group/item rejection stages SHALL conservatively retain it unless the primitive supplies valid conservative bounds. Material changes affecting this contract SHALL invalidate spatial bounds before collection.

#### Scenario: Triangle in clip space
- **WHEN** Triangle uses a clip-space transform independent of the camera
- **THEN** its world-space BVH eligibility and final rendering preserve that coordinate contract

#### Scenario: Switch to a vertex displacement usage
- **WHEN** an object previously culled by static bounds switches to a usage requiring unknown displaced bounds
- **THEN** old group and primitive bounds cannot prevent collection or drawing based on an invalid static bound
