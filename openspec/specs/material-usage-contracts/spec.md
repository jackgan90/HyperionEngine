# material-usage-contracts Specification

## Purpose
Define shared built-in material usage and variant names while preserving custom string extension points.
## Requirements
### Requirement: Well-known material names are shared and extensible
Materials SHALL declare the engine's well-known usage and variant spellings for shared consumers. Custom usage and variant strings SHALL remain supported. Execution-mode shader defines SHALL retain their existing authoritative mapping and values.

#### Scenario: Existing and custom usages
- **WHEN** built-in material construction, render routing or a custom pass selects a usage/variant
- **THEN** built-in spellings match their historical values and unknown custom names remain usable through existing extension points
