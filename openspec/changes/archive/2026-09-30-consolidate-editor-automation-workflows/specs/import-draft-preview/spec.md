## ADDED Requirements

### Requirement: Semantic import choices and typed preview metadata
Import GUI choices and applicable settings SHALL resolve stable capability type IDs rather than numeric positions. Texture draft inspection SHALL expose reflected typed dimensions and pixel byte size shared by GUI and automation. GUI SHALL not parse human-readable Details to recover those fields. Preview pagination SHALL use an explicit consistent limit for requests and navigation.

#### Scenario: Capability ordering changes
- **WHEN** supported import capabilities are presented in a different order
- **THEN** selecting a type still produces its correct stable type ID, allowed source filters and applicable settings without adding unsupported formats

#### Scenario: Inspect a texture through either surface
- **WHEN** GUI or automation inspects a prepared texture draft
- **THEN** both observe matching typed dimensions and byte size, existing Details remains compatible, and discovery describes the additive fields

#### Scenario: Multiple pages of preview nodes
- **WHEN** the user advances and returns through preview pages
- **THEN** query limits and navigation offsets agree with no skipped or repeated page caused by inconsistent limits
