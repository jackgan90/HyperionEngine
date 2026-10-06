# shader-pipeline Specification

## Purpose
TBD - created by archiving change add-shader-compilation-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Portable shader artifacts
The engine SHALL compile HLSL vertex, pixel and compute shaders to DXIL and SPIR-V and generate MSL source through an internal wrapper. Each artifact SHALL expose normalized resource and member reflection for its actual compilation target or preserved intermediate source, including cbuffer type trees, sizes, offsets, array/matrix strides and major order, binding spaces/counts/stages and shader input/output signatures. Compute reflection SHALL include fixed thread-group dimensions and distinguish sampled resources from writable texture/structured/raw resources. Native compiler and reflection types SHALL remain private. MSL reflection SHALL retain the generated resource mapping without implying a Metal execution backend.

#### Scenario: Valid shader
- **WHEN** the triangle shader is compiled for each target
- **THEN** DXIL and SPIR-V artifacts are nonempty and SPIR-V can be reflected and converted to MSL, with normalized reflection available on all three artifact paths

#### Scenario: Complete member reflection
- **WHEN** a shader contains nested uniform structures, arrays, non-square matrices and resources in multiple spaces
- **THEN** callers can identify typed member paths and their actual target layout without inferring offsets from C++ structures or another target

#### Scenario: Binding conflicts and visibility
- **WHEN** vertex and pixel artifacts are combined into a material program
- **THEN** compatible bindings are merged, disjoint stage-specific declarations remain distinguishable, and overlapping incompatible declarations produce diagnostics

#### Scenario: Compute cache and reflection
- **WHEN** a compute shader with storage resources is compiled cold and from cache for each supported artifact target
- **THEN** stage, writable resource kinds, group dimensions, member layout and final bytes are preserved consistently

### Requirement: Correct cache invalidation
The compiler SHALL key cached artifacts by the toolchain, compilation options and source-root contents and reject corrupted cache bytes. Explicit defines SHALL be normalized with duplicate names rejected. Artifact/reflection schema and register-space mapping versions SHALL be part of cache identity. Cache hits SHALL reconstruct the same normalized reflection and final bytes as cold compilation, preserving the unstripped DXIL or SPIR-V intermediate required for reflection and MSL conversion. Actual applicable profile, HLSL language version, target environment and other represented compilation-policy inputs SHALL participate directly in identity; an explicit version SHALL remain for non-described processing changes. Immutable virtual-include names/bytes and logical-reflection policy inputs SHALL remain represented. Presentation-only information SHALL NOT create cache misses.

#### Scenario: Include modification
- **WHEN** an included file changes after an initial compilation
- **THEN** the next artifact has a different cache key and is compiled again

#### Scenario: Cache hit with reflection
- **WHEN** a DXIL, SPIR-V or MSL shader is compiled again from a valid cache
- **THEN** its resource/member reflection and final bytes equal the cold result

#### Scenario: Option and mapping changes
- **WHEN** defines or the binding/reflection format version changes with unchanged source text
- **THEN** cache identity changes and stale interface data is not reused

#### Scenario: Actual target policy changes
- **WHEN** only the applicable target profile, HLSL version or SPIR-V environment changes with otherwise identical inputs
- **THEN** the production cache identity changes together with the corresponding compilation arguments

#### Scenario: Restored policy
- **WHEN** compilation repeats with identical applicable policy and source inputs
- **THEN** a valid cache entry is reused with equal artifact/reflection data and without replaying compiler diagnostics

#### Scenario: MSL payload semantics
- **WHEN** an MSL request hits its valid SPIR-V intermediate cache
- **THEN** current MSL conversion runs from the preserved intermediate and is not treated as a cached final MSL string

### Requirement: Actionable diagnostics
The compiler SHALL propagate compilation diagnostics to engine callers.
#### Scenario: Invalid source
- **WHEN** invalid HLSL is compiled
- **THEN** the operation fails with a diagnostic containing the shader error.

### Requirement: Mounted text shader sources
Shaders SHALL remain plain text assets and compile through engine-owned filesystem access for main sources and includes. Engine and Game shader paths SHALL be supported, including Game includes of Engine shared files. Content relocation SHALL not change cache identity when logical source contents and compilation inputs are unchanged.

#### Scenario: External material shader
- **WHEN** a Game text shader includes an Engine shader header
- **THEN** supported targets compile and reflect through the same pipeline without local path references in the material

#### Scenario: Relocated warm cache
- **WHEN** mount physical locations change with identical logical contents
- **THEN** the same shader cache entry remains valid and changed includes still invalidate it

### Requirement: Structured resource element metadata
Shader reflection SHALL expose structured-buffer element metadata needed by engine resource contracts, including element stride and available numeric/structure/array member types and offsets. Reflection/cache versions SHALL change when this metadata changes. Native adapter types SHALL remain private.

#### Scenario: Structured light element reflection
- **WHEN** a shader uses a structured light record
- **THEN** the engine artifact carries sufficient element metadata to distinguish same-stride incompatible records

#### Scenario: Cached artifact after reflection change
- **WHEN** an artifact was produced under an older reflection metadata version
- **THEN** it is not reused as an artifact with the new element contract

### Requirement: Immutable generated shader declarations
Shader compilation SHALL support immutable virtual includes generated from engine parameter metadata. Include names and bytes SHALL participate in shader cache identity and remain owned throughout compilation. Generated declarations SHALL retain layout validation through native reflection.

#### Scenario: Changed generated declaration
- **WHEN** a generated uniform declaration changes while authored shader files remain unchanged
- **THEN** the old compiled artifact is not reused

#### Scenario: Generated layout mismatch
- **WHEN** an authored shader declaration conflicts with its expected engine contract
- **THEN** compilation or material preparation reports the mismatch before GPU submission

### Requirement: Generated uniform types for explicit shader resources
The declaration generator SHALL expose native HLSL uniform structure types from the readable owner declaration frontend. Owner-local shaders SHALL be able to declare cbuffers explicitly using those types and select registers in shader source without repeating individual fields. Common flat targets and existing instance-array protocols SHALL retain compatible declarations where required. This path SHALL use the existing generated virtual-include mechanism without a shader annotation parser or C++ header-processing tool. Owned declaration formatting SHALL be idempotent and SHALL preserve following top-level shader indentation.

#### Scenario: Explicit Contact cbuffer
- **WHEN** ContactShadows includes ContactShadowParameters.generated.hlsli and declares ContactV1 with FContactV1Uniform ContactShadow at b0
- **THEN** ContactShadow.Viewport and the other generated fields are available, and native reflection supplies the actual register and layout for validation

#### Scenario: Formatting readable declarations
- **WHEN** the formatter runs repeatedly over uniform BEGIN/END declarations and shaders that consume generated types
- **THEN** the files remain unchanged after the first formatting pass and following top-level resources and functions retain their correct indentation

#### Scenario: Derived declaration changes
- **WHEN** a field type, order, packing convention or compatibility constraint changes generated include bytes or contract metadata
- **THEN** shader and prepared-program cache identity prevents incompatible reuse, with representation versions updated when required

### Requirement: Shared shader register protocol
Shaders SHALL own a typed register-class protocol shared by compiler shift arguments, reflection decoding and resource-range checks. Register class SHALL be independent of resource-kind enumeration. Existing b/t/s/u offsets, four supported spaces, 1000 registers per class and mapping version SHALL remain compatible. Unknown classes, invalid spaces/registers, unbounded or zero counts and arrays crossing the supported class range SHALL fail explicitly without overflow.

#### Scenario: Fixed mapping compatibility
- **WHEN** register 7 is encoded for b, t, s and u in any supported space
- **THEN** binding values are respectively 7, 1007, 2007 and 3007, the space is preserved and decoding recovers the original class/register

#### Scenario: Array boundary rejection
- **WHEN** a resource starts at register 999 with count 2, uses space 4 or has zero/unbounded count
- **THEN** validation rejects the range rather than accepting a collision with another class

#### Scenario: Readonly and writable storage resources
- **WHEN** reflection encounters structured or raw resources represented by the target's storage-buffer family
- **THEN** the decoded register class distinguishes t from u and existing logical resource-kind reconciliation remains accurate

### Requirement: One applicable shader compilation policy
The compiler SHALL construct target-specific arguments, selected-profile diagnostics and cache policy inputs from the same applicable target description. The description SHALL preserve supported default profiles, language version and target environment without introducing a public override requirement. Native adapter types SHALL remain private and immutable source snapshots SHALL remain shared by payload and logical-reflection compilation.

#### Scenario: Policy and diagnostics agree
- **WHEN** a selected target profile changes in the compiler's target policy
- **THEN** actual DXC arguments, diagnostic profile and cache identity describe that same selected profile

#### Scenario: Inapplicable policy field
- **WHEN** only a SPIR-V environment or a display label changes while compiling DXIL
- **THEN** DXIL arguments and cache identity remain unchanged

### Requirement: Independent recoverable shader storage
Shader compilation SHALL consume the reusable derived-data store using its existing semantic compilation identity. Default application cache paths SHALL be independent of out. Cache location changes SHALL NOT alter semantic shader keys or reflection. Storage failures SHALL permit successful cold compilation results to be used.

#### Scenario: Cold cache after deletion
- **WHEN** the local cache is deleted between runs with valid source/toolchain inputs
- **THEN** shaders recompile with equivalent output/reflection and user configuration remains intact

#### Scenario: Cache relocation
- **WHEN** a valid store is relocated and inputs remain identical
- **THEN** compilation can reuse its records with the same semantic keys
