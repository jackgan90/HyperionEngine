## ADDED Requirements

### Requirement: Optional contact visibility before lighting
Deferred SHALL allow requested HZB generation and a fullscreen contact mask after opaque/masked BasePass and before lighting. Both ordinary and clustered directional lighting SHALL sample the mask and combine visibility with CSM. Compatibility, sky, transparency and shared output ordering SHALL remain intact; disabled contact SHALL use neutral visibility and request no HZB.

#### Scenario: Contact-enabled Deferred frame
- **WHEN** an eligible main directional light and contact request exist
- **THEN** BasePass depth precedes compute HZB, HZB precedes mask generation, and mask generation precedes directional lighting

#### Scenario: Forward or later surfaces
- **WHEN** Forward is selected or a surface is shaded after Deferred lighting
- **THEN** that route retains its existing shadow evaluation without sampling a mask belonging to another receiver surface
