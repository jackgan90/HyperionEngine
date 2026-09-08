# instance-uniform-rendering Specification

## Purpose
Define reflected typed instance records, system instance-ID access, capability-bounded native draws and immutable data lifetime with ordinary-rendering fallback.

## Requirements
### Requirement: Reflected instance record mapping
Material preparation SHALL validate explicit instance-array declarations against shader reflection and map record members to ordinary typed logical parameters with unchanged scope, override and required-input semantics. Compiled programs SHALL expose record layout and capacity independently of shader filenames. Undeclared shaders SHALL remain usable for count-one draws.

#### Scenario: Custom record types
- **WHEN** a custom shader declares a record containing numeric vectors, matrices, nested structures and fixed arrays
- **THEN** each item supplies its own typed values and packing uses reflected offsets and strides

#### Scenario: Invalid declaration
- **WHEN** an instance contract names a missing block/member or a malformed or inconsistent array layout
- **THEN** the optional instance permutation is rejected with an explicit diagnostic before drawing, while a valid ordinary permutation remains available

### Requirement: System instance ID uniform access
Built-in Model, Triangle and GUI shaders SHALL access their per-instance numeric inputs through system instance IDs and constant-buffer records. Ordinary draws SHALL compile with `HYP_ENABLE_INSTANCE=0`; batches SHALL compile with `HYP_ENABLE_INSTANCE=1`. Shader support SHALL require reflected system instance-ID input and valid record arrays, not only macro text. Shaders that omit instance support or force the macro to zero SHALL fall back to ordinary draws without requiring a shader metadata system. Pixel-stage instance access SHALL preserve an integer instance index without interpolation. No instance-ID vertex stream SHALL be required.

#### Scenario: Macro disabled or absent
- **WHEN** an ordinary shader ignores the injected macro or fixes it to zero
- **THEN** its valid ordinary program remains usable and it is not selected for batching

#### Scenario: Different instance materials
- **WHEN** two model instances have different transforms and surface factors
- **THEN** VS and PS fetch the matching instance records and produce the same image as count-one draws

### Requirement: Capability-bounded indexed instance draws
RHI SHALL expose usable instanced drawing and validate positive instance count, program capacity, required constant slice extent and device binding/range/alignment limits. Renderer SHALL split larger groups into bounded chunks. Unsupported batch execution SHALL provide a diagnostic and retain valid ordinary rendering where possible.

#### Scenario: Capacity boundary
- **WHEN** compatible visible items exceed a shader's instance-array capacity
- **THEN** multiple draws cover every item exactly once without reading outside published data

#### Scenario: Invalid packet extent
- **WHEN** an instanced packet lacks enough constant records or exceeds its declared capacity
- **THEN** RHI rejects it before native recording

### Requirement: Immutable instance data lifetime
Instance constant storage SHALL be immutable after publication and SHALL remain alive through every referencing recorded/submitted frame. Reuse SHALL only reset a page after all cache, packet and GPU owners have released it.

#### Scenario: Mutation with an old frame retained
- **WHEN** instance membership or values change while an older frame is retained or in flight
- **THEN** the new frame uses new data and the old frame continues to render its original contents

#### Scenario: Cancellation and shutdown
- **WHEN** frame recording, Present or resource preparation fails, or the scene closes
- **THEN** instance data follows existing cancellation/fence retirement rules and no stale resource is reused
