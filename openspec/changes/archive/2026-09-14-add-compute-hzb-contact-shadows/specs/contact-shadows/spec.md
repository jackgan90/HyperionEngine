## ADDED Requirements

### Requirement: Directional hierarchical screen tracing
Renderer SHALL generate a full-resolution contact visibility mask for valid Deferred opaque/masked points by tracing finite rays toward the selected main directional light using HZB traversal and full-resolution depth confirmation. Reconstruction, thickness and bias SHALL respect camera/viewport and both depth conventions. Offscreen, invalid and unresolved samples SHALL yield neutral visibility with bounded execution.

#### Scenario: Small contact occluder
- **WHEN** a visible small object occludes the ray from a nearby valid surface toward the light
- **THEN** the mask reduces visibility near the contact and avoids self-shadowing an unobstructed isolated surface in both conventions

### Requirement: Independent contact visibility composition
Contact visibility SHALL use 1 for lit and 0 for occluded and combine with CSM using min in both depth conventions. The result SHALL affect only selected directional direct lighting. Contact and CSM effect switches SHALL be independent while main-light shadow eligibility remains respected.

#### Scenario: CSM disabled
- **WHEN** contact is enabled for an eligible light and CSM is disabled
- **THEN** contact remains effective with neutral CSM visibility

#### Scenario: Indirect light preservation
- **WHEN** only contact shadow visibility changes
- **THEN** environment, ambient, emissive and unrelated local-light contributions remain unchanged

### Requirement: Dynamic controls and activation evidence
DebugUI SHALL dynamically control contact enablement and tracing parameters through frozen frame settings and expose mask/HZB diagnostics and active work statistics. Contact SHALL be inactive in Forward and on later compatibility/transparent surfaces. When contact is the only consumer, disabling it SHALL remove HZB and mask generation.

#### Scenario: Runtime toggle and pipeline change
- **WHEN** contact is toggled or Deferred switches to Forward while frames are in flight
- **THEN** new frames follow the new settings, old frames retain their published inputs, and inactive frames perform no contact-driven HZB generation

### Requirement: Sponza acceptance and measured costs
Delivery SHALL demonstrate the effect on the current ready SceneViewer Sponza scene with GUI switching, both depth conventions, static/moving cameras and Debug/Release validation. Evidence SHALL distinguish contact-mask behavior from CSM artifacts that minimum visibility cannot correct and report measured per-pass GPU costs and resource stability.

#### Scenario: Accepted scene run
- **WHEN** Sponza assets and material bindings are ready and contact is toggled under a fixed camera/light
- **THEN** captures show localized directional contact detail, no invalid-resource errors occur, and activation/timing evidence identifies the actual generated work
