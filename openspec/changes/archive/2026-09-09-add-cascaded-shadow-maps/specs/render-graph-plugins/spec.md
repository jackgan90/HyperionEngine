## MODIFIED Requirements

### Requirement: Limited depth and stencil attachment validation
The graphics graph SHALL validate depth and stencil initialization separately for configured D32 or D32S8 targets and explicit sampled D32 offscreen attachments. Zero or one color attachment SHALL be supported. Stencil operations SHALL require a stencil-capable attachment with initialized contents before load. This change SHALL NOT imply multiple color targets or resolve operations.

#### Scenario: Stencil unavailable
- **WHEN** a pass requires stencil while the configured depth target has no stencil component
- **THEN** validation fails before recording with the target incompatibility

#### Scenario: Undefined stencil load
- **WHEN** the first stencil-using pass attempts to load uncleared stencil contents
- **THEN** graph compilation fails; an explicit first clear followed by later loads is accepted

#### Scenario: Offscreen depth-only pass
- **WHEN** a pass clears and writes an explicit depth texture with no color attachment
- **THEN** the actual texture dimensions, depth format and zero-color pipeline contract are validated without binding or clearing swapchain color

## ADDED Requirements

### Requirement: Explicit sampled depth dependencies
The graph SHALL track explicit depth writes and shader reads, produce compatible resource transitions and reject undefined or conflicting accesses. Native lists SHALL retain attachments and barrier resources through fence completion, including empty draws and failure cleanup.

#### Scenario: Shadow producer and consumer
- **WHEN** several depth-only passes are followed by a forward pass sampling their targets
- **THEN** writes execute before reads with correct barriers and no simultaneous incompatible binding

#### Scenario: Cancelled clear-only frame
- **WHEN** a frame containing a retained offscreen clear is cancelled or fails after submission
- **THEN** resource lifetime follows submission completion and the next valid frame can initialize and sample the target safely
