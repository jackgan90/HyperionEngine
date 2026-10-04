## ADDED Requirements

### Requirement: Typed material sampler choices
Material sampler address and comparison controls SHALL resolve selections through actual supported enum values rather than presentation indices. Existing values, display labels, validation, history and persistence SHALL remain compatible.

#### Scenario: Reordered sampler presentation
- **WHEN** sampler options are displayed in another order
- **THEN** selecting an option preserves its address or comparison meaning and stored numeric value
