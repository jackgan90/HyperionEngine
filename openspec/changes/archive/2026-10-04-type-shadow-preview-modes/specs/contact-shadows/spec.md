## ADDED Requirements

### Requirement: Explicit contact preview interpretation
Contact shadow consumers SHALL use validated typed identities for lit, visibility-mask and HZB-depth preview. Existing disabled-feature, absent-light, pipeline and mip behavior SHALL remain unchanged.

#### Scenario: HZB preview
- **WHEN** HZB-depth preview is selected
- **THEN** the previous depth request and mip preview occur independently of the visibility-mask branch

#### Scenario: Invalid contact mode
- **WHEN** a value outside the supported contact preview contract is provided
- **THEN** shared settings validation rejects it without changing the active settings
