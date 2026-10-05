## ADDED Requirements

### Requirement: Accepted passes have typed payloads

RenderGraph SHALL own accepted pass scheduling facts separately from typed graphics/compute payloads. Compute-only fields MUST NOT be stored in graphics declarations. Kind and resource views MUST derive from the active payload. Public Add/AddCompute, timing, copy/consume and stable scheduling semantics SHALL remain available.

#### Scenario: Graphics and compute graph executes
- **WHEN** declared graphics, compute and buffer passes are compiled and executed
- **THEN** their plans, dependencies, resource states, packet ownership and submission behavior remain equivalent

### Requirement: Single authoritative accepted color list

Graph admission SHALL normalize legacy Color into its accepted color list, reject simultaneous Color/Colors without admitting a pass, and retain the requested native command layout. Internal validators MUST consume the normalized attachment list.

#### Scenario: Ambiguous color declaration
- **WHEN** a caller supplies both Color and Colors
- **THEN** Add fails before changing graph membership or running any resolver/Prepare

#### Scenario: Legacy single and MRT declarations
- **WHEN** valid legacy single-color or color-vector declarations are compiled
- **THEN** load/store, viewport, sRGB views, DrawBatch policy and native command shapes remain unchanged

### Requirement: Validation remains staged before side effects

Graph-wide declaration/content errors MUST fail before resource resolution or deferred Prepare. Resolved resource errors MUST fail before deferred Prepare. Invalid callback-produced commands MUST fail after the relevant callback but before BeginFrame/submission. Failed compilation and successful consuming execution SHALL preserve existing ownership and reuse rules.

#### Scenario: Late declared error
- **WHEN** a later pass has invalid dependencies, resource accesses or undefined contents
- **THEN** no earlier resolver, Prepare or native frame/submission side effect occurs

#### Scenario: Invalid resolved resource or deferred command
- **WHEN** physical resolution is invalid or Prepare produces an invalid draw/dispatch
- **THEN** rejection occurs at its defined stage with verified callback/frame/submission counts and no false success
