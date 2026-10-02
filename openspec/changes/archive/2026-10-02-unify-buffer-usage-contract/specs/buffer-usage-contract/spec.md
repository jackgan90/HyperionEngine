## ADDED Requirements

### Requirement: RHI-owned buffer usage foundation

RHI SHALL own the known buffer usage membership, shader read/write categories, and typed composition/query operations. The sets SHALL be composed from the declared enum values instead of independently encoded numeric masks in consumers. Existing enum values, `BufferUsage(ERHIBufferUsage)` calls and public `uint32_t` usage storage SHALL remain compatible. Creation and graph import SHALL reject zero and unknown usage bits before accepting the resource.

#### Scenario: Existing combinations retain meaning
- **WHEN** a caller composes existing Vertex, Index, Constant, StructuredRead, RawRead, StructuredWrite and RawWrite values through the shared foundation
- **THEN** the resulting numeric values and membership results match the existing bit contract without changing descriptor storage or callers' existing single-value conversion

#### Scenario: Unknown usage bit
- **WHEN** a create or import request contains an unknown bit alone or combined with known bits
- **THEN** validation rejects the request instead of discarding that bit or treating it as a known category

### Requirement: Explicit graph buffer support and state compatibility

RenderGraph SHALL explicitly restrict buffer imports to a nonempty subset of StructuredRead, RawRead, StructuredWrite and RawWrite. Graph SHALL continue to allow ShaderRead state for any valid buffer import, including write-only usage, and SHALL require a write usage for ShaderWrite state. This state-level contract SHALL NOT replace native access/view validation. Existing source, size, identity, alias, initialization, overwrite, compute-only write and dependency/barrier checks SHALL remain in force.

#### Scenario: Write-only buffer read state
- **WHEN** a valid graph buffer import has only StructuredWrite or RawWrite usage and declares ShaderRead as its initial or exported state
- **THEN** graph state validation accepts the existing compatible declaration while actual native reads remain subject to their own usage checks

#### Scenario: Unsupported graph usage or write state
- **WHEN** a graph import contains Vertex, Index or Constant usage, or a read-only graph buffer is requested in ShaderWrite state
- **THEN** Graph rejects the unsupported declaration without widening its resource support

#### Scenario: Repeated compute write
- **WHEN** valid compute passes write the same initialized buffer consecutively
- **THEN** Graph retains the required UAV ordering barrier and rejects undefined-content use when no full overwrite establishes initialization

### Requirement: Backend-specific buffer restrictions remain local

D3D12 SHALL consume the shared known/read/write membership while retaining its own allocation and access restrictions. Storage buffer creation SHALL exclude Vertex, Index and Constant and require four-byte alignment. Constant buffers SHALL retain exclusive Constant usage and 256-byte size alignment. Native ShaderRead accesses SHALL require a read usage; ShaderWrite accesses SHALL require a write usage and compute execution. Native buffer transitions and diagnostic readback SHALL remain restricted to storage buffers carrying a write usage. Diagnostic readback SHALL preserve its existing acceptance of write-only storage, independently of shader read access. Size, initial-data, device identity, view/range and simultaneous-access validation SHALL remain unchanged.

#### Scenario: Native creation compatibility
- **WHEN** requests enumerate the existing usage combinations at otherwise valid sizes
- **THEN** D3D12 accepts the existing geometry/read, exclusive constant and shader storage combinations and rejects combinations that mix storage with geometry or constant usage

#### Scenario: Native alignment and data restrictions
- **WHEN** a storage size is not a multiple of four, a constant size is not a multiple of 256, or initial bytes exceed the buffer size
- **THEN** native creation rejects the request independently of known usage membership

#### Scenario: State acceptance does not bypass native read validation
- **WHEN** a native buffer has write-only usage and a command declares actual ShaderRead access
- **THEN** D3D12 rejects that access even if a Graph ShaderRead state declaration was accepted

#### Scenario: Storage-only transition and diagnostic readback
- **WHEN** a readonly buffer is requested for native buffer transitions or diagnostic readback
- **THEN** D3D12 rejects that operation, while valid write-only storage retains transition support and returns its expected bytes through diagnostic readback

### Requirement: Consistent Renderer material buffer usage derivation

Renderer SHALL derive the RHI usages of a material buffer source at one private domain boundary shared by compute graph declarations, graphics buffer-read declarations and native material buffer allocation. Read-only sources SHALL preserve StructuredRead and RawRead usage; storage sources SHALL additionally preserve StructuredWrite and RawWrite usage. Materials SHALL remain independent of RHI, and the translation SHALL NOT change source identity, immutable snapshots, deferred resolution, resource-cache ownership, RHI execution domains or GPU retirement.

#### Scenario: Source imported and allocated
- **WHEN** a read-only or storage material buffer is declared for compute or graphics and resolved to a native buffer
- **THEN** graph import usage and native immutable buffer information agree using the shared translation, and existing physical-description mismatch checks remain active

#### Scenario: Previously published resource remains live
- **WHEN** a newer material source or frame replaces a buffer while older work still retains the previous source or GPU resource
- **THEN** previous bytes, resource identity and fence-gated lifetime remain valid through that work's completion

### Requirement: Independent layered compatibility verification

Verification SHALL compare the RHI foundation, Graph import/state behavior and D3D12 creation/access behavior against independent fixed expectations. It SHALL cover every combination of the seven existing bits, zero, unknown high bits and the distinct state/access acceptance rules. Production composition/query helpers SHALL NOT be the sole oracle for their own expected results. Supported native compute/readback and lifetime regressions SHALL remain executable.

#### Scenario: A category is changed accidentally
- **WHEN** a production usage set or query stops matching the existing bit contract
- **THEN** the independent compatibility matrix identifies the affected layer and combination rather than agreeing through the same shared helper

#### Scenario: A valid native case fails for an unrelated reason
- **WHEN** a supported buffer case cannot create or execute because of device or resource failure
- **THEN** verification reports failure or an explicit environment limitation and does not count it as a correct invalid-usage rejection
