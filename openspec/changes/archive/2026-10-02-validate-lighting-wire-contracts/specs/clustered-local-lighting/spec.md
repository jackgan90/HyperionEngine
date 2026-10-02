## ADDED Requirements

### Requirement: Explicit shared local-light encoding

Clustered and volume-light producers SHALL consume shared named CPU encoding of inverse range, inner/outer cone cosine and spot classification. HLSL consumers SHALL decode those meanings through named fields or helpers while preserving the existing packed layouts, numerical attenuation and spot/range cutoff behavior.

#### Scenario: Point and spot compatibility
- **WHEN** asymmetric point and spot inputs are encoded for clustered and volume paths
- **THEN** independent expected words and GPU output confirm the same range/cone meaning, with exact 0/1 spot encoding and unchanged attenuation boundaries

### Requirement: Lighting wire publication remains immutable

Wire validation and encoding SHALL preserve unchanged-buffer reuse, radiance-independent cluster assignment and queued-frame byte ownership. Additional directional-light selection, empty records and source reuse SHALL remain compatible.

#### Scenario: Radiance-only change and queued old frame
- **WHEN** radiance changes without bounds changes and a previous frame is still retained
- **THEN** assignment lists are not rebuilt, unchanged header/index sources are reused and the previous frame's source bytes remain unchanged

#### Scenario: Directional filtering and empty resources
- **WHEN** primary, disabled or nonpositive-radiance directional lights are considered, or producers have no lights
- **THEN** existing filtering, one-record empty representation and identical-byte source reuse remain unchanged
