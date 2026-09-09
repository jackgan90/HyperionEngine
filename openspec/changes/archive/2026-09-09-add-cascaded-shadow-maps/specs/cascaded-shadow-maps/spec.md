## ADDED Requirements

### Requirement: Stable directional cascaded shadows
Renderer SHALL support one shadowed directional light with up to four finite depth cascades, configurable resolution/distance, stable projections, bounded filtering/bias, overlap blending and far-distance fading. Every active cascade SHALL update every frame.

#### Scenario: Moving camera and extreme light
- **WHEN** the camera translates/rotates and the light is vertical, horizontal or parallel/opposed to the camera
- **THEN** projections remain finite, coverage stays conservative and shadows have no abrupt basis flips, invalid sampling or unbounded bias

### Requirement: Independent shadow caster visibility
Each shadow view SHALL query the shared scene independently from main-view visibility and include offscreen casters that affect its receiver region. Empty updated maps SHALL be cleared and invalid bounds SHALL fail open for culling.

#### Scenario: Offscreen caster and removal
- **WHEN** an offscreen object casts onto visible ground and is subsequently moved, hidden or removed
- **THEN** the ground shadow is initially present and changes on the next rendered frame without stale depth

### Requirement: Shared model shader permutations
Forward and shadow-caster programs SHALL compile from the same Model.hlsl source with a shadow-caster macro and shared transform/alpha evaluation. Opaque casters SHALL omit PS; masked casters SHALL only alpha-test; both SHALL support ordinary and instanced draws. Blended materials SHALL receive but not cast shadows in this version.

#### Scenario: Masked and mirrored instances
- **WHEN** masked, double-sided or mirrored instances render into a shadow view
- **THEN** silhouette, alpha cutoff and winding match the visible geometry and batching does not alter the shadow image

### Requirement: Direct lighting shadow reception
Builtin lit model pixels SHALL choose cascades using linear camera depth and apply visibility only to the shadowed directional direct lighting. Ambient, emissive and unlit behavior SHALL remain unchanged by shadow reception.

#### Scenario: Contact and sloped receivers
- **WHEN** opaque objects contact a floor or cast onto sloped surfaces under grazing light
- **THEN** bounded depth/normal/receiver bias and PCF suppress acne while retaining contact, without corrupting ambient or emissive terms

### Requirement: Bounded and measured shadow cost
Delivery SHALL include reproducible warmed off/on CPU/GPU measurements, individual cascade timings, forward sampling increment, resource counts and sustained-motion checks. Stable frames SHALL reuse textures, shaders, PSOs and descriptors; normal rendering SHALL NOT introduce shadow-specific idle waits or synchronous image readback.

#### Scenario: Sustained shadow rendering
- **WHEN** the viewer renders thousands of frames with camera/light motion after warmup
- **THEN** resource counts remain bounded, every active cascade updates and timing results identify any regressions against documented configuration and targets
