## ADDED Requirements

### Requirement: Domain-owned environment bake admission

Runtime/Environment SHALL own the authoritative numeric settings rules used for panorama baking and its specular prefilter subset. Radiance size SHALL remain a power of two from 1 through 1024, specular size a power of two from 1 through 256 not exceeding radiance, and samples from 1 through 1024. Standalone prefiltering SHALL retain its separate source-cube admission without adding the panorama radiance cap. Existing bake output, defaults and invalid_argument diagnostics SHALL remain compatible.

#### Scenario: Boundary and incompatible settings
- **WHEN** a caller validates minimum/maximum settings or supplies zero, non-power-of-two, oversized or specular-larger-than-radiance settings
- **THEN** the shared predicate and actual baker agree on numeric admission before allocating bake products

#### Scenario: Captured cube input
- **WHEN** standalone prefiltering validates an otherwise valid linear cube wider than 1024 with supported output size and samples
- **THEN** the panorama radiance-size cap does not reject that input

#### Scenario: Invalid image data
- **WHEN** settings are valid but panorama dimensions, storage or radiance values are invalid
- **THEN** the baker retains its image validation and rejection behavior
