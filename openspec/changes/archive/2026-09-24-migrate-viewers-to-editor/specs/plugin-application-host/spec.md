## MODIFIED Requirements

### Requirement: Plugin-owned application features
Editor SHALL run as an application feature plugin using reusable plugin-owned asset, window, graphics and GUI services. Application entrypoints SHALL supply configuration and compiled catalogs. Native backend selection SHALL remain in application composition.

#### Scenario: Editor startup
- **WHEN** either application starts with its normal profile
- **THEN** provider dependencies govern startup and reverse teardown while existing CLI and rendering behavior remain available
