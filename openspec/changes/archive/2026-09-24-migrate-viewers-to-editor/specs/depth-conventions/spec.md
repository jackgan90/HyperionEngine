## MODIFIED Requirements

### Requirement: Startup depth configuration
Editor SHALL default its reversedZ setting to true and freeze the active depth convention at startup. Configuration edits SHALL be saveable for the next launch without changing active rendering.

#### Scenario: Missing setting
- **WHEN** an existing configuration omits reversedZ
- **THEN** Editor and its scene/asset viewports use reversed-Z

#### Scenario: Standard fallback and pending edits
- **WHEN** reversedZ is false at startup and later edited to true
- **THEN** all submitted frames remain standard-Z and saving persists true for the next launch
