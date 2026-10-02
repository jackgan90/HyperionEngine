## ADDED Requirements

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

## MODIFIED Requirements

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
