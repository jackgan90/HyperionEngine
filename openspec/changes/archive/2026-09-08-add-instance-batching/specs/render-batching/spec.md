## ADDED Requirements

### Requirement: Render-owned strategy coordination
Renderer SHALL build batch plans on Render after visibility filtering and material resolution through an engine-owned strategy interface. Plans SHALL own their frame data and leases, and RHI SHALL materialize native work. Primitive inheritance and Main Scene SHALL remain independent of batching strategy implementations.

#### Scenario: Alternative strategy
- **WHEN** another strategy is registered through the generic coordinator
- **THEN** it participates through the same compatibility and coverage protocol without changing primitive subclasses

### Requirement: Effective instance compatibility
Instance batching SHALL compare geometry identity and range, program and physical layout, effective graphics state, resource bindings including views/samplers, shared constants and ordering domain. Per-instance numeric values and material instance identity SHALL NOT alone prevent merging. Hash hits SHALL be verified by complete equality.

#### Scenario: Different numeric overrides
- **WHEN** compatible items use different colors, matrices or typed record values but the same effective resources
- **THEN** one instanced draw supplies the distinct values to their respective instances

#### Scenario: Similar bindings are not equal
- **WHEN** items differ in bound texture, sampler, buffer view, index range, mirrored pipeline orientation or dynamic state
- **THEN** incompatible items remain in distinct draws or batches

### Requirement: Exclusive coverage and order preservation
Within a frame/family/view/pass, each valid source item SHALL be emitted exactly once, either by one strategy or by ordinary fallback. Cross-view/pass rendering SHALL remain legal. Reordering SHALL require explicit pass permission and SHALL NOT cross order-sensitive barriers.

#### Scenario: Competing strategies and fallback
- **WHEN** multiple strategies accept an item or batch preparation falls back
- **THEN** final publication contains exactly one representation of that item

#### Scenario: Transparent and state barriers
- **WHEN** transparent, overlay or order-sensitive work separates otherwise compatible items
- **THEN** batching preserves its existing order and does not merge across that barrier

### Requirement: Stable compact cache reuse
The system SHALL cache immutable compact visible chunks under bounded entry and byte budgets. Matching SHALL account for source identity, actual emitted contents, layout, membership/order and effective values. Anonymous item ordinals SHALL NOT establish cross-frame identity.

#### Scenario: Static and view-only changes
- **WHEN** visible members and instance values are unchanged, including a view-only update independent of instance values
- **THEN** instance data and its GPU slice are reused

#### Scenario: Visibility and numeric changes
- **WHEN** an item enters/leaves visibility or changes an instance value
- **THEN** affected chunks are updated with only current visible instances while unrelated chunks remain reusable

#### Scenario: Removal and cache pressure
- **WHEN** primitives are removed, generations change or cache budgets are exceeded
- **THEN** stale entries retire without aliasing new items, retaining unbounded history or invalidating in-flight frames

### Requirement: Group-atomic batch publication
The system SHALL preserve whole-model group failure behavior and per-item diagnostics. A failed source group SHALL contribute no member to any published batch, while independent valid groups SHALL continue. Provisional batched and fallback draws SHALL NOT both publish.

#### Scenario: Failure in another section
- **WHEN** one section of a model fails preparation while another section shares a batch with a healthy model
- **THEN** all failed-model members are excluded and the healthy model still renders
