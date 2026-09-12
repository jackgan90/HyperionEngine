## ADDED Requirements

### Requirement: Scene camera and light jointly determine cascades
For scene-bound rendering, CSM SHALL derive its main camera description and selected directional light from the same resolved publication used by forward/deferred lighting and geometry collection. Scene light enabled state, nonzero radiance and cast-shadows flag SHALL combine with the pipeline shadow enable setting. When that combination disables shadows, new frames SHALL use neutral shadow reception and SHALL NOT display stale cascade data as active.

#### Scenario: Camera and light change together
- **WHEN** a parented camera and selected directional light change before frame submission
- **THEN** cascade setup, receiver sampling, direct lighting and caster queries use the same publication and correct surface-to-light convention

#### Scenario: Disable only cast shadows
- **WHEN** the selected light stops casting shadows while its radiance remains nonzero
- **THEN** direct light remains unchanged and shadow views/reception are disabled without changing pipeline quality settings

#### Scenario: Remove all lighting
- **WHEN** the selected light is removed, disabled or has zero radiance
- **THEN** no active cascades remain for that light and old depth maps cannot darken subsequent frames
