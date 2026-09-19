## ADDED Requirements

### Requirement: Staged rendering contributions
The renderer SHALL expose named feature stages with graph, immutable frame, view and explicit resource context. Contributions SHALL declare RenderGraph accesses and respect exact view and resource lifetimes. Registration SHALL be fixed before frame execution.

#### Scenario: Intermediate feature
- **WHEN** an enabled feature consumes Deferred depth before lighting
- **THEN** it contributes at the corresponding stage and lighting can consume its published result without host-specific pass insertion

### Requirement: Optional contact-shadow feature
Contact shadows SHALL use the render-feature extension path, preserve consumer-driven hierarchical depth and disappear when unavailable or disabled. Existing Forward and scene-shadow capability restrictions SHALL remain enforced.

#### Scenario: Feature absent
- **WHEN** contact-shadow support is not registered
- **THEN** ordinary lighting remains functional and contact-shadow passes and exclusive resources are not produced
