# graphics-resource-binding Specification

## Purpose
Define engine-owned graphics binding layouts, immutable resource sets, complete pipeline-state composition, executable capability limits and validated resource retention through GPU completion.
## Requirements
### Requirement: General graphics binding layouts and sets
RHI SHALL expose engine-owned binding layouts, immutable binding sets, samplers and buffer slices without material/model-specific fields. Layouts SHALL describe kind, count, stage visibility and binding space/location. Sets SHALL retain all referenced resources. Constant slices SHALL be independently bindable without duplicating unchanged texture/sampler sets per object.

#### Scenario: More than five textures
- **WHEN** a valid material uses eight sampled 2D textures and independent samplers within reported limits
- **THEN** the draw binds them correctly without a fixed five-resource layout or depth requirement

#### Scenario: Stage-specific bindings
- **WHEN** VS and PS use nonzero spaces and compatible shared or disjoint stage-specific bindings
- **THEN** the native layout preserves their reflected identity and visibility, rejecting only overlapping incompatible declarations

### Requirement: Resource binding set caching
The coordinator SHALL cache resource binding sets by device/layout/group and ordered resource identity/generation/view properties and normalized sampler values. Independent dynamic CBV slices SHALL NOT cause otherwise unchanged texture/sampler descriptor tables to be rebuilt. Cache records SHALL retire after their live CPU/GPU references end.

#### Scenario: Stable material over many draws and frames
- **WHEN** multiple objects only change Object constants while retaining the same effective resource bindings
- **THEN** after warmup the same binding set and descriptor ranges are reused and descriptor allocation/copy counts do not grow per draw or frame

#### Scenario: Resource view or array changes
- **WHEN** a resource array element order, buffer view range, texture view or sampler changes
- **THEN** only the affected set is replaced, the new binding is correct, and the old set/range remains valid until its gated GPU users complete

### Requirement: Explicit graphics state and target compatibility
Graphics pipelines SHALL describe rasterization, depth/stencil, RGB/alpha blend equations, write masks and sample state. Draw commands SHALL explicitly carry supported dynamic state. Final pipeline identity SHALL include shader, layout, vertex interface/topology and target format/sample compatibility. Single-sample D32 and configured D32S8 targets SHALL support depth-disabled and stencil-capable paths respectively.

#### Scenario: Sampler or uniform value changes
- **WHEN** only a dynamic sampler, texture reference or uniform value changes with unchanged shader/layout/static state
- **THEN** the existing compatible pipeline is reused

#### Scenario: Pipeline state differs
- **WHEN** two draws differ in depth compare, blend factors, winding or target format
- **THEN** their distinct effective states are preserved and incompatible PSOs are not reused

#### Scenario: Stencil state executes
- **WHEN** a D32S8 fixture writes stencil with one draw and tests it with another using configured reference/masks
- **THEN** the resulting pixels follow the declared front/back stencil operations and no undefined stencil contents are read

### Requirement: Bounded executable resource capabilities
The device SHALL report enabled binding kinds, shader stages, register spaces, counts, constant alignment/range and descriptor capacities. Graphics support SHALL include fixed resource arrays, 2D RGBA8 sampled textures, sampled D32 depth targets, independent ordinary/comparison samplers and validated read-only structured/raw buffer views. VS texture sampling SHALL use a resource state valid for that stage. Unsupported mandatory features SHALL fail before submission.

#### Scenario: Read-only buffer view
- **WHEN** a shader reads a valid structured or raw buffer view
- **THEN** the view's usage, stride, extent and alignment are validated and the expected bytes reach the shader

#### Scenario: Vertex texture sampling
- **WHEN** a vertex shader samples an uploaded 2D texture
- **THEN** its descriptor is visible to VS and the uploaded resource is ready in a compatible shader-resource state

#### Scenario: Comparison sampler request
- **WHEN** a supported pipeline requires SamplerComparisonState for a sampled depth texture
- **THEN** reflection, layout and sampler comparison state are validated and execute the requested comparison

### Requirement: Binding validation and failure isolation
Before recording draws, the backend SHALL validate resource/device identity, layout compatibility, complete active slots, type/count/stage, upload readiness, constant range/alignment, geometry ranges and target compatibility. Partial creation failures SHALL roll back unpublished allocations without releasing submitted upload retention.

#### Scenario: Invalid constant slice
- **WHEN** a packet has an unaligned, overflowing or undersized constant buffer slice
- **THEN** recording fails explicitly before native draw commands use that slice

#### Scenario: Foreign binding set
- **WHEN** a packet references a binding set, sampler or nested resource from another device
- **THEN** validation rejects it consistently with existing foreign-resource checks

### Requirement: Descriptor and parameter resource retention
D3D12 layouts SHALL respect the root-signature budget and use device-owned descriptor heaps with checked capacity and range allocation. Published descriptor ranges SHALL remain immutable until retained draws and GPU submissions finish. Renderer-managed layout, sampler, binding-set and constant-page destruction SHALL occur through the existing RHI 0 coordinator. Direct RHI clients SHALL retain their existing device-state lifetime contract.

#### Scenario: Descriptor exhaustion
- **WHEN** a controlled small heap cannot satisfy a binding set allocation
- **THEN** creation reports capacity exhaustion, partially reserved ranges are reclaimed, and already published sets remain valid

#### Scenario: Last frame fails at Present
- **WHEN** a frame using material descriptors has submitted GPU work before Present fails
- **THEN** descriptor ranges, buffers and samplers remain retained until the submitted work completes and can retire without another presented frame

### Requirement: Recording-local redundant binding elimination
The native graphics backend SHALL avoid re-emitting identical binding state within a command-list recording when it remains valid. Tracking SHALL be isolated per recording context and invalidated by relevant list, root-signature and descriptor-heap boundaries. Validation and GPU retention requirements SHALL remain in force for all draws.

#### Scenario: Consecutive compatible draws
- **WHEN** consecutive draws share heaps, root signature and some constant or descriptor bindings
- **THEN** unchanged valid bindings are not re-emitted and differing slots are correctly updated

#### Scenario: State boundary and failure recovery
- **WHEN** a new list starts, root or heap state changes, or recording resumes after cancellation
- **THEN** all required bindings are established without borrowing stale tracked state from another recording

### Requirement: Renderer-produced texture sources
Engine-owned CPU resource descriptions SHALL identify renderer-produced sampled depth targets without exposing RHI/native objects to Scene or Materials. Renderer SHALL resolve the same identity for attachment writes and material reads and preserve descriptor identity across content updates.

#### Scenario: Reuse a rendered texture
- **WHEN** a depth source is rendered again with unchanged dimensions and binding layout
- **THEN** its texture and binding sets are reused while subsequent consumers see the new depth contents
