# engine-shader-semantics Specification

## Purpose
Define common and owner-local typed shader contracts, readable shared declarations, independently derived GPU layouts and demand-driven native reflection validation across graphics, fullscreen and compute.

## Requirements

### Requirement: Typed common and owner-local semantic declarations
The engine SHALL declare common global/frame/view inputs through EngineSemantics.inl and rendering-domain inputs through owner-local macro declarations using the same mechanism. Local fields SHALL NOT extend the common global enum. Runtime builtin identities SHALL be strongly typed and SHALL NOT own string identities. String shader/asset names SHALL be resolved at interface boundaries; custom names SHALL remain extensible.

#### Scenario: Builtin input publication
- **WHEN** engine code publishes Frame time or a View input
- **THEN** it uses a generated builtin identifier and an unknown identifier is a C++ compile error

#### Scenario: New scoped uniform
- **WHEN** a developer adds a uniform contract in any supported scope
- **THEN** the same declaration mechanism produces identity, metadata and layout without a material-specific mapping branch

#### Scenario: Independent domain declarations
- **WHEN** Deferred lighting and Contact shadows declare fields with the same spelling
- **THEN** each uses its own typed identity and qualified shader instance, and neither declaration is added to EngineSemantics.inl

#### Scenario: Shader-local contract selection
- **WHEN** a graphics or compute factory supplies an owner-local contract set
- **THEN** generic preparation generates its shader include and validates only reflected resources from the common and supplied sets without depending on the owning subsystem

### Requirement: Readable single-field contract declarations
The author-facing macro frontend SHALL declare each numeric uniform field once with shader type, field name and update scope. It SHALL derive typed C++ fields, semantic descriptors and shader structure fields from that declaration without separate numeric-semantic and uniform-member entries. Owner declaration files SHALL NOT require visitor-selection conditional blocks. Resource declarations SHALL produce typed resource identities through the same frontend. Default policies SHALL be implicit; exceptional policy and stable wire aliases MAY be supplied as metadata referring to declared fields.

#### Scenario: Contact uniform authoring
- **WHEN** a developer reads ContactShadowParameters.inl
- **THEN** HYP_UNIFORM_BEGIN(ContactV1, ContactShadow), its ordered HYP_UNIFORM_FIELD declarations and HYP_UNIFORM_END expose the uniform fields without manual offsets, total size or duplicate semantic entries

#### Scenario: Existing semantic alias
- **WHEN** the derived declaration identity differs from a shipped semantic wire name
- **THEN** explicit compatibility metadata preserves that name without duplicating the field's type, scope or layout declaration

### Requirement: Automatically derived expected uniform layout
The engine SHALL infer expected uniform offsets, matrix/array strides and total size from ordered field declarations and documented supported GPU packing rules. Update scope SHALL NOT affect packing. Expected layout SHALL be independent of native reflection and C++ object sizeof/offsetof and SHALL be immutable before value publication. GPU bool SHALL occupy four bytes under the supported uniform profile. Unsupported types or packing combinations and layout arithmetic overflow SHALL fail with contextual diagnostics. Structured-buffer packing SHALL use its own profile. Exceptional fixed-ABI gaps or offsets MAY be represented explicitly without making ordinary fields require numeric layout arguments.

#### Scenario: Contact inferred layout
- **WHEN** the accepted Contact fields are declared in their existing order with default column-major float4x4 matrices
- **THEN** the inferred offsets are 0, 64, 128, 144, 160, 172, 176, 180, 184 and 188 respectively, and total size is 192 bytes

#### Scenario: Scope-independent layout
- **WHEN** a field changes update scope without changing its type, order or explicit ABI constraints
- **THEN** its GPU offset and the uniform size remain unchanged while provider and invalidation behavior use the new scope

#### Scenario: Preserved historical gap
- **WHEN** migration encounters a deliberate historical gap that ordinary packing would remove
- **THEN** explicit compatibility metadata preserves the existing layout and stable binding targets

#### Scenario: Unsupported packing declaration
- **WHEN** a declaration uses a type or matrix/array convention without a supported packing rule
- **THEN** contract construction rejects it rather than inferring a layout from C++ storage or native reflection

#### Scenario: Native target mismatch
- **WHEN** an encountered native shader resource differs from the independently inferred contract
- **THEN** preparation rejects it before submission using the complete reflected contract validator

### Requirement: Demand-driven resource contracts
Preparation SHALL discover builtin contracts by reflected resource name, validate only resources encountered in a compiled variant, and derive bindings from common metadata and the material's selected owner-local contract sets. The physical register and space SHALL come from reflection. Unknown custom resources SHALL retain explicit parameter authoring; incompatible reserved engine resources SHALL fail with contextual diagnostics.

#### Scenario: Cluster consumer without manual mapping
- **WHEN** a shader declares the cluster-view contract at a supported register/space without C++ member targets
- **THEN** preparation validates the contract and binds the cluster inputs automatically

#### Scenario: Material without cluster resources
- **WHEN** a material variant has no reflected cluster resource
- **THEN** no cluster provider or contract validation is required for that variant

### Requirement: Complete reflected contract validation
Encountered uniform contracts SHALL compare reflected scalar types, shapes, offsets, array and matrix strides, major order and extents against expected layouts. Resource contracts SHALL validate kind, count, dimension, comparison mode and structured element layout. Validation SHALL distinguish unavailable declaration metadata from validated metadata, and SHALL NOT fabricate evidence for optimized-out resources.

#### Scenario: Same-sized incompatible member
- **WHEN** a contract replaces a float member with a uint at the same offset
- **THEN** preparation rejects the layout before GPU submission

#### Scenario: Structured element mismatch
- **WHEN** a structured resource retains its element byte size but changes a field type or order
- **THEN** its engine contract validation rejects the resource

### Requirement: Shared preparation across execution paths
Graphics, fullscreen and compute SHALL use the same engine contract lookup and validation. Engine-owned builtin value mapping SHALL derive from catalog metadata rather than feature-specific shader member string rewriting. Explicit custom pass parameters SHALL remain usable.

#### Scenario: Fullscreen builtin scene resources
- **WHEN** deferred lighting consumes the environment or cluster contract
- **THEN** frozen typed inputs are mapped through the shared contract metadata

#### Scenario: Compute contract validation
- **WHEN** compute declares an incompatible engine uniform contract
- **THEN** preparation rejects it using the same contract rules as graphics

### Requirement: Typed builtin shader parameter structures
The catalog SHALL generate typed C++ parameter structures and uniform contract identifiers. Builtin shader/pass inputs SHALL use structures, semantic IDs or prepared handles rather than shader parameter name strings. Numeric and resource values SHALL retain typed identity through submission. Shared declaration generation SHALL process shader inputs only and SHALL NOT require a C++ header-processing tool.

#### Scenario: Builtin pass assignment
- **WHEN** a builtin lighting, sky, depth, preview or outline pass publishes parameters
- **THEN** its C++ code assigns typed fields and resources without spelling a shader member path

#### Scenario: HZB layout variants
- **WHEN** copy and reduction compute shaders use different constant layouts
- **THEN** they use distinct validated contracts and preserve resource access and mip selection

#### Scenario: Conditional lighting use
- **WHEN** the shader disables directional lighting
- **THEN** the shared uniform declaration remains compatible and unused inputs do not require automatic providers

### Requirement: Metadata-driven builtin policies
Builtin ownership, grouping, authorability and editing behavior SHALL use catalog metadata rather than semantic-name prefixes or parameter-name substrings. Authored parameter names SHALL remain independent from their builtin semantic.

#### Scenario: Renamed PBR parameter
- **WHEN** an authored metallic, roughness, UV or color parameter has a different logical name
- **THEN** builtin preview and editing behavior is selected by its semantic rather than the spelling of that name

### Requirement: Validated CPU structured wire records

Direct CPU structured-buffer uploads for cluster lights, cluster headers, cluster indices and additional directional lights SHALL validate their supported physical member types, names, offsets, extents and record stride against the existing owner-declared shader contract before upload. Expected layouts SHALL remain independent of C++ storage and native reflection. Binding strides SHALL come from the validated contract. Validation SHALL require standard-layout/trivially-copyable records and SHALL NOT silently accept unsupported bool, matrix or array storage.

#### Scenario: Same-size CPU field mismatch
- **WHEN** two same-typed fields are reordered or a member's physical type/offset changes while the record's total size remains unchanged
- **THEN** CPU wire validation rejects the mismatch before publishing an upload source

#### Scenario: Header and index adapters
- **WHEN** cluster headers or indices are uploaded
- **THEN** the header's typed Offset/Count pair is validated as contiguous uint32 values at offsets 0/4 against the unnamed uint2 contract, and the index is validated against the unnamed uint contract
- **AND** their binding strides remain eight and four bytes respectively

#### Scenario: Existing lighting ABI
- **WHEN** current supported cluster and directional values are encoded
- **THEN** their 64-byte and 32-byte records, semantic IDs and contract versions remain unchanged and independent field readback agrees with fixed expected values

#### Scenario: Setup and target validation boundaries
- **WHEN** immutable lighting contracts are initialized and shader variants are prepared
- **THEN** CPU layout validation runs at setup while existing target reflection validation remains required, without per-light descriptor construction or string lookup
