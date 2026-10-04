## ADDED Requirements

### Requirement: Typed light diagnostic identity
Renderer and Editor SHALL produce and branch on typed known light diagnostic kinds. A single Renderer-owned mapping SHALL define the established `directional` and `sky` tokens.

#### Scenario: Native diagnostic production
- **WHEN** a directional or environment light is included in the lighting snapshot
- **THEN** its known kind is represented by the corresponding enum and its external token remains `directional` or `sky` respectively
- **AND** selection, eligibility, priority, tie status, messages and sky asset status remain unchanged

### Requirement: Lossless diagnostic protocol compatibility
The diagnostic `type` field SHALL retain its string schema, empty default, field ID, record version and archive representation. All previously accepted strings SHALL round trip without truncation or normalization; unknown strings SHALL have no known native kind.

#### Scenario: Known and opaque values
- **WHEN** known, unknown, empty, differently cased, whitespace-bearing or embedded-NUL strings are decoded and encoded
- **THEN** their exact values are preserved through wire and archive round trips
- **AND** numeric or null values remain invalid and an omitted field retains the empty default

#### Scenario: Automation discovery
- **WHEN** clients describe or invoke `scene.lighting.get`
- **THEN** existing operation metadata, schemas and output tokens remain compatible without transport-specific changes

### Requirement: Member-based light Inspector routing
Editor SHALL identify Priority and Sky asset properties using their reflected C++ member associations within the owning component descriptor. Existing tooltip text, tone, warnings and status labels SHALL be preserved.

#### Scenario: Field identity survives display and wire spelling changes
- **WHEN** a copied descriptor renames the ID or label of an associated light member
- **THEN** its resolved role and presentation remain the same
- **AND** an unrelated field with a matching old spelling does not acquire that role

#### Scenario: Diagnostic presentation
- **WHEN** a matching light is selected, overridden, disabled, tied, loading or failed
- **THEN** its property presentation retains the existing messages, tones and labels
- **AND** unrelated handles, unknown kinds and non-light properties remain unaffected
