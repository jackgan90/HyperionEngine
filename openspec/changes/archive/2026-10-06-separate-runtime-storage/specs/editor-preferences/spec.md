## ADDED Requirements

### Requirement: User-editable storage locations
Editor preference SHALL display active and saved user-data/cache locations and the bootstrap locator. It SHALL allow editing both roots through the shared revisioned storage service and explain restart and launch-override behavior. Typed automation SHALL expose the same validation/persistence operation without GUI simulation.

#### Scenario: GUI and automation equivalence
- **WHEN** either entry point submits the same root edit
- **THEN** normalization, writability checks, revision checks, persistence and restart state are identical

#### Scenario: Missing optional adapter
- **WHEN** storage automation is disabled
- **THEN** GUI storage preferences remain usable and unrelated automation remains available
