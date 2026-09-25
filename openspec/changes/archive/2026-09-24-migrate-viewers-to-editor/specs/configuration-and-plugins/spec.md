## MODIFIED Requirements

### Requirement: Explicit activation authority
Persisted feature parameters SHALL NOT implicitly enable plugins. Explicit CLI feature selection SHALL update requested selection while honoring disabled-plugin configuration. Supported plugin IDs and serialized property keys SHALL remain stable; retired application-specific configuration is removed explicitly.

#### Scenario: Disabled optional feature
- **WHEN** a saved setting requests an explicitly disabled optional feature
- **THEN** the feature is not activated and an attempted dependent operation reports unavailability
