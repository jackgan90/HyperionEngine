## ADDED Requirements

### Requirement: CPU control contract ownership

The engine SHALL expose all seven existing renderer control interfaces and their public DTOs through an owned CPU target. Authoritative identity, reflection and defaults MUST remain single definitions. That target's complete production dependency closure MUST exclude Renderer, RHI, native backends and concrete plugins.

#### Scenario: Independent control consumer
- **WHEN** a consumer links only the control target and includes its seven interface headers
- **THEN** settings, viewport, shadow/light, output, diagnostics and capture values and reflection compile and link without renderer implementation dependencies

#### Scenario: Forbidden indirect dependency
- **WHEN** a control target reaches rendering implementation through another target
- **THEN** dependency validation rejects the complete path and reports its links and declaration locations

### Requirement: Explicit production adapters

Pipeline construction/conversion, configuration persistence, device/task sampling, readback/file output and complete render view snapshots SHALL remain with their production owners. The control target MUST NOT expose those implementation helpers. Existing renderer include paths SHALL re-export the same CPU definitions during migration.

#### Scenario: Complete frame remains available
- **WHEN** Renderer produces frame statistics for an Editor camera consumer
- **THEN** that consumer retains the complete camera view while control diagnostics uses CPU statistics with unchanged reflected fields

#### Scenario: Numeric viewport compatibility
- **WHEN** a shadow control supplies a preview viewport
- **THEN** the existing numeric viewport type, fields, equality and defaults remain unchanged without introducing an RHI dependency into the control target

### Requirement: Existing operations remain compatible

The engine SHALL preserve reflected TypeIds and field schemas, operation IDs, defaults, revisions, errors, availability and async completion/cancellation semantics. GUI and automation SHALL continue through their existing shared domain providers.

#### Scenario: Existing control invocation
- **WHEN** a supported provider receives an existing control request through automation or GUI
- **THEN** its values, transaction/revision result and errors retain the previous behavior

#### Scenario: Missing control provider
- **WHEN** a host lacks a control provider or it has been withdrawn
- **THEN** discovery and invocation retain their controlled unavailable behavior without accessing a destroyed provider

### Requirement: Consumer migration evidence

Automation SHALL consume the CPU contract entry points and cease directly linking Renderer after all seven contracts migrate. Validation MUST distinguish that direct boundary from application aggregate dependencies.

#### Scenario: Application aggregate still renders
- **WHEN** Automation's direct Renderer link is removed but ApplicationServices still reaches Renderer
- **THEN** the contract consumer boundary is verified and no CPU-only claim is made for the entire application executable
