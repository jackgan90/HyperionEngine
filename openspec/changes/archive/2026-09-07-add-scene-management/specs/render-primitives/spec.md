## ADDED Requirements

### Requirement: Optional pre-collection bounds
Render primitives SHALL provide an optional conservative world bound through a Render-only read interface independently of Collect output cardinality. The default for custom primitives SHALL be unknown and eligible for collection. Related registrations SHALL support Render-owned culling groups without making a group a draw submission unit.

#### Scenario: Candidate group with multiple outputs
- **WHEN** a visible group contains a primitive emitting multiple items
- **THEN** the existing zero-to-many collection and scene-wide pass path accepts all emitted items

#### Scenario: Rejected group
- **WHEN** a conservative group bound is wholly outside the culling frustum
- **THEN** no member Collect method is called for that view
