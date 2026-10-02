## ADDED Requirements

### Requirement: Shared built-in material route facts

Renderer SHALL define unchanged built-in Usage strings, pipeline eligibility, pick eligibility and legacy-exclusion membership in one finite route description consumed by scene view construction and ray-pick options. Target/resource construction, stable view identities and pass order SHALL remain explicit and compatible. Custom Usage strings SHALL remain supported independently of this built-in inventory.

#### Scenario: Drawing and picking use compatible routes
- **WHEN** a material contains legacy, HDR, mixed, shadow-only or custom usages
- **THEN** Deferred and Forward drawing and picking match their independently specified existing route matrix without duplicate contributions or shadow/fullscreen picking

#### Scenario: Inactive HDR usage still excludes legacy
- **WHEN** a material has Forward fallback plus an HDR usage not active in the current pipeline
- **THEN** that HDR membership still excludes the material from legacy fallback under the complete four-usage exclusion set

#### Scenario: Resource and stage ordering
- **WHEN** a built-in scene frame is constructed from route descriptions
- **THEN** shadow, BasePass/Forward, compatibility, transparency and display target construction and execution preserve their existing order and resource lifetime contracts
