## MODIFIED Requirements

### Requirement: Consistent sky and lighting
Builtin lighting SHALL evaluate diffuse SH and split-sum GGX specular IBL from the same sky generation and common yaw, intensity and tint as the sky background, using a shared BRDF integration LUT. Tint SHALL multiply each color channel of sky radiance for the background, diffuse SH and specular IBL, and SHALL NOT affect direct, local or emissive light. Tint, intensity and yaw edits SHALL update rendering parameters without reloading sky resources. Forward, Deferred, clustered and transparent paths SHALL use the common implementation, without fixed ambient contribution in sky mode. Emissive and unlit behavior SHALL remain independent.

#### Scenario: Rotate and replace sky
- **WHEN** an environment is rotated or replaced
- **THEN** the background, diffuse directionality and reflections change together, without mixing dependency generations

#### Scenario: No directional light
- **WHEN** the scene selects only a sky environment with clustered lighting enabled
- **THEN** diffuse and specular IBL remain active and agree with Forward rendering

#### Scenario: Tint a sky
- **WHEN** a SkyAsset environment's tint changes from white to a single color channel
- **THEN** the background, diffuse and specular contributions retain only that channel, direct and emissive lighting are unchanged, and no sky reload is started
