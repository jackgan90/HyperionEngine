## MODIFIED Requirements

### Requirement: Limited depth and stencil attachment validation
The graphics graph SHALL validate depth and stencil initialization separately for configured D32 or D32S8 targets and explicit sampled D32 offscreen attachments. Zero through the supported maximum ordered color attachments SHALL be supported, including sampled offscreen color targets with explicit formats. Stencil operations SHALL require a stencil-capable attachment with initialized contents before load. Every color attachment SHALL have independently validated dimensions, format and content operations. This change SHALL NOT imply resolve operations or simultaneous sampled and writable depth use.

#### Scenario: Stencil unavailable
- **WHEN** a pass requires stencil while the configured depth target has no stencil component
- **THEN** validation fails before recording with the target incompatibility

#### Scenario: Undefined stencil load
- **WHEN** the first stencil-using pass attempts to load uncleared stencil contents
- **THEN** graph compilation fails; an explicit first clear followed by later loads is accepted

#### Scenario: Offscreen depth-only pass
- **WHEN** a pass clears and writes an explicit depth texture with no color attachment
- **THEN** the actual texture dimensions, depth format and zero-color pipeline contract are validated without binding or clearing swapchain color

#### Scenario: GBuffer producer and consumer
- **WHEN** a BasePass writes multiple color targets that Lighting samples
- **THEN** all targets participate in RAW/WAR/WAW dependencies and transitions, and undefined, aliased or dimension-incompatible attachments are rejected

## ADDED Requirements

### Requirement: Sampled color target lifetime
Graph color imports SHALL identify immutable physical format, dimensions, initial state and content validity. Exports SHALL establish the requested reusable final state. Recorded lists SHALL retain every color attachment and transition resource through submitted completion including clear-only and failed frames.

#### Scenario: Replaced target generation
- **WHEN** resize or layout replacement occurs while an old color target is retained by submitted work
- **THEN** the old resource is not released until that work completes and the new graph uses only its declared target generation
