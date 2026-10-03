## ADDED Requirements

### Requirement: Explicit preview option and product identity

Asset preview shapes and texture channels SHALL have typed internal identities with a single authoritative mapping to their existing wire values, captions and geometry/component semantics. GUI selection order SHALL NOT define those semantics. Sky preview products SHALL associate their source references, cached textures and captions through named members and explicit mappings. Texture format captions SHALL be selected by explicit format identity. These mappings SHALL preserve existing preview defaults, ordering, labels, rendering, preparation ownership and invalidation behavior.

#### Scenario: Presentation changes preserve selection meaning
- **WHEN** a preview option presentation is reordered or relabelled
- **THEN** selecting a shape or channel resolves the same typed identity, existing wire value and geometry/component independently of its GUI position or caption

#### Scenario: Sky product correspondence
- **WHEN** a sky asset's distinct Radiance, Specular and Brdf products are prepared and displayed
- **THEN** each product remains associated with its own named reference, texture and caption without matching parallel array positions

#### Scenario: Same preview selection preserves state
- **WHEN** an existing preview shape or channel is selected again
- **THEN** its semantic state remains unchanged and no redundant preview rebuild is requested
- **AND** preview-only changes do not modify asset generation, dirty state, undo history or native asset contents

#### Scenario: Channel display compatibility
- **WHEN** RGBA, red, green, blue or alpha is selected
- **THEN** the preview uses the same component and existing exposure, alpha background and conversion behavior as before the typed contract
