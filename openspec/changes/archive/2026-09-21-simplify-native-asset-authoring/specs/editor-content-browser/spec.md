## MODIFIED Requirements

### Requirement: Internal asset visibility
Editor SHALL show all native project assets by default without an internal-assets toggle. Git, source caches and publication temporaries SHALL be excluded from browsing and scene discovery. Asset classification SHALL continue to use stored TypeId; unsupported editors SHALL report controlled messages without disturbing the scene.

#### Scenario: Browse native dependencies
- **WHEN** a migrated project contains model, material, texture, sky and scene hassets
- **THEN** all files are browsable without an opt-in visibility setting and Git/cache folders remain excluded
