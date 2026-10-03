## ADDED Requirements

### Requirement: Stable asset preview option wire contract

The asset.preview.get/set operation IDs, reflected request/result schemas and preview readiness/generation contracts SHALL remain unchanged when internal option identities are typed. Shape values SHALL remain 0=Sphere, 1=Plane and 2=Cube; channel values SHALL remain 0=RGBA, 1=R, 2=G, 3=B and 4=A. GUI and automation SHALL continue to use the same UI-independent preview setting service, which SHALL validate option values through the authoritative mapping before mutation. Unsupported values SHALL be rejected without silently choosing a default or altering preview/document state.

#### Scenario: Existing client round trip
- **WHEN** an attached client discovers and invokes asset.preview.set with each supported shape/channel number
- **THEN** asset.preview.get reports the same number and the preview exhibits the corresponding existing behavior
- **AND** schema identities, field types and null/omitted patch semantics remain compatible

#### Scenario: Invalid option rejection
- **WHEN** a client supplies an unknown shape or channel number to a ready supported preview
- **THEN** the request fails as invalid_arguments before changing settings, asset generation, dirty state or history

#### Scenario: Unsupported and unavailable previews
- **WHEN** an option is unsupported for the asset type or the preview service is unavailable
- **THEN** the existing controlled failure behavior is preserved without adding transport-specific domain logic
