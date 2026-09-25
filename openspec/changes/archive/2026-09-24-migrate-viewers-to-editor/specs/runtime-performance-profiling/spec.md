## ADDED Requirements

### Requirement: Editor profiling controls
Editor SHALL expose session-local CLI and GUI controls for profiling with visible compiled/active/connected state. Unsupported profiling requests SHALL fail clearly. Capture windows SHALL support warmup and a bounded number of frames without changing scene or asset persistence or workload behavior.

#### Scenario: Toggle during interaction
- **WHEN** the user changes the profiling controls
- **THEN** the displayed state and subsequent eligible events reflect that change without restarting the rendering application

## MODIFIED Requirements

### Requirement: Material optimization evidence
The engine SHALL expose low-overhead material work and cache statistics sufficient to distinguish shared reuse, actual evaluation, expensive constant lookup, packing/upload and bounded cache retention. Optimization verification SHALL record reproducible before/after frame timing and comparable workload/build metadata and SHALL include engine-level frequency and cache-pressure tests independent of the Editor application.

#### Scenario: Before and after comparison
- **WHEN** material performance improvements are reported
- **THEN** Debug and optimized build results identify warmup, sample counts, draw counts, instrumentation and validation settings, and preserve underlying frame CSV and profiling evidence

#### Scenario: Generality validation
- **WHEN** a non-builtin material exercises different scopes and prolonged cache pressure through engine APIs
- **THEN** counters demonstrate the intended update frequency and bounded cache retention without application-specific fast paths

#### Scenario: Closed cache statistics
- **WHEN** resource service closure has destroyed the material constant cache and its pages
- **THEN** live page count and page capacity statistics are zero while cumulative work counters remain available

## REMOVED Requirements

### Requirement: Viewer profiling controls
**Reason**: The independent application contract is retired; shared capabilities are owned by Editor and Runtime.
**Migration**: Use the Editor workflows and shared services specified by this change.
