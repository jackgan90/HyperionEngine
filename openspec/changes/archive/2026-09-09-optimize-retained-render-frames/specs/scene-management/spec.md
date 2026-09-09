## ADDED Requirements

### Requirement: Unchanged logical scenes avoid full bridge scans
The Main scene bridge SHALL process logical changes and affected editable material revisions incrementally. An unchanged scene without editable material selections SHALL avoid per-instance publication preparation. Readiness and error caching SHALL invalidate on relevant scene and resource publications.

#### Scenario: Ready immutable scene
- **WHEN** an unchanged scene with frozen/inherited materials is flushed repeatedly
- **THEN** the bridge publishes no new primitive work and does not rebuild per-model material-version lists

#### Scenario: Shared editable material changes
- **WHEN** an editable material used by multiple models changes without a logical model update
- **THEN** all affected selections synchronize atomically with their current geometry and overrides
