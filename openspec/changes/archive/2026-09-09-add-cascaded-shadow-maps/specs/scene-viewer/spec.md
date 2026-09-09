## ADDED Requirements

### Requirement: Shadow demonstration and controls
SceneViewer SHALL demonstrate directional shadows on its ground and models, expose enable/light direction/resolution/distance/bias and cascade visualization controls, and show per-view draw/culling and shadow timing diagnostics using engine GUI APIs.

#### Scenario: Inspect shadows interactively
- **WHEN** the user changes the light direction or moves/hides a model
- **THEN** all active cascades update on the next frame and the visible scene and diagnostics reflect the change

### Requirement: Shadow validation fixtures
Delivery SHALL include deterministic contact, sloped, masked, thin/double-sided, offscreen-caster, cascade-transition and extreme-light fixtures plus static/moving performance comparisons.

#### Scenario: Shadow correctness suite
- **WHEN** CPU, GPU and viewer shadow acceptance tests run
- **THEN** depth sampling, main/shadow view separation, shadow reception, empty-map clearing and enabled/disabled/batching comparisons are verified with documented image and performance evidence
