## MODIFIED Requirements

### Requirement: Bounded and measured shadow cost
Delivery SHALL include reproducible warmed off/on CPU/GPU measurements, individual cascade timings, forward sampling increment, resource counts and sustained-motion checks. Stable frames SHALL reuse textures, shaders, PSOs and descriptors; normal rendering SHALL NOT introduce shadow-specific idle waits or synchronous image readback.

#### Scenario: Sustained shadow rendering
- **WHEN** the application renders thousands of frames with camera/light motion after warmup
- **THEN** resource counts remain bounded, every active cascade updates and timing results identify any regressions against documented configuration and targets
