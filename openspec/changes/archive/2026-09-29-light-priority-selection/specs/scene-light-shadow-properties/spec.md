## MODIFIED Requirements

### Requirement: Published light shadow precedence
Renderer SHALL consume authored shadow values from the priority-resolved directional shadow source in the immutable scene publication. Authored values SHALL take precedence over session defaults; a source without overrides SHALL restore defaults. Contact shadows and CSM SHALL use the same source with their independent method switches. Host-only preview placement SHALL not be serialized into light data.

#### Scenario: Switch priority winner
- **WHEN** priorities select a different eligible light with or without authored shadow settings
- **THEN** both shadow consumers use its direction and settings without leaking previous values
