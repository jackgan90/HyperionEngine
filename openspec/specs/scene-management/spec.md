# scene-management Specification

## Purpose
Define renderer-independent Main scene ownership, stable model instance handles, shared immutable assets and ordered rendering synchronization.

## Requirements
### Requirement: Renderer-independent logical scene
The Scene module SHALL own flat logical model instances independently of Renderer and RHI. Handles SHALL include scene identity, slot and generation. Main mutations SHALL support add, remove, asset attachment, transform, visibility and material override with monotonic revisions and owned incremental snapshots.

#### Scenario: Reuse and foreign handles
- **WHEN** an old or foreign handle is used after slot reuse
- **THEN** the current instance remains unchanged

#### Scenario: Shared asset independent state
- **WHEN** two instances reference one immutable asset and one is moved or hidden
- **THEN** the other retains its state and both share immutable asset data

### Requirement: Ordered rendering attachment
A Main rendering bridge SHALL publish complete instance state through the existing ordered Render client. Model member changes and removal SHALL be frame-atomic. Render SHALL NOT borrow Main mutable objects. Accepted removals SHALL progress without presentation and preserve existing frame leases.

#### Scenario: Initial transform and removal
- **WHEN** a transformed model is attached and subsequently removed while a snapshot exists
- **THEN** no identity-transform frame is exposed and the old snapshot remains valid after removal

#### Scenario: Late asset result
- **WHEN** loading finishes after the logical instance was removed
- **THEN** it cannot attach an asset or register primitives for a reused slot

#### Scenario: Minimized scene
- **WHEN** scene mutations occur while presentation is paused
- **THEN** the bridge and Render control queue continue applying changes and cleanup
