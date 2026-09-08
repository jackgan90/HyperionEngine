# material-parameter-binding Specification

## Purpose
Define typed uniform-name and semantic access, prepared schemas, versioned parameter scopes, target-aware packing and lifetime-safe sharing of compatible GPU uniform storage.

## Requirements
### Requirement: Uniform names and semantic mappings address typed parameters
The material system SHALL expose active uniform/resource names as typed parameters and SHALL support explicit mappings between parameter paths and registered semantics. Name and semantic access SHALL resolve to the same logical parameter storage. Full stage/block/member paths SHALL disambiguate repeated names. Unmapped active shader parameters SHALL remain accessible by name.

#### Scenario: Arbitrary shader spelling
- **WHEN** a shader's ViewData.Eye is mapped to Engine.View.CameraPosition and a custom DetailStrength has no semantic
- **THEN** the engine supplies Eye from the view and callers can change DetailStrength through its reflected uniform path

#### Scenario: Ambiguous short name
- **WHEN** two shader targets expose the same short name with different logical parameters
- **THEN** short-name access fails with the qualified alternatives and full-path access selects exactly one parameter

#### Scenario: Type mismatch
- **WHEN** a caller supplies an incompatible scalar/vector/matrix/array shape or resource kind
- **THEN** the write fails without altering values or revision

### Requirement: Prepared CPU schema publication
Renderer material interface preparation SHALL compile and reflect on Worker independently of GPU/frame readiness, convert results to Materials-owned CPU schema, and publish an immutable versioned interface for Main to accept. Automatic parameter discovery SHALL complete before an instance uses discovered parameter handles. Unknown-name writes before interface readiness SHALL return InterfaceNotReady. Cross-pass/variant schema merging SHALL reject incompatible types and retain per-variant active mappings. Workers SHALL NOT mutate existing Main instances or definitions.

#### Scenario: Reflection-only custom uniform
- **WHEN** an authored definition omits a schema entry for an active shader uniform
- **THEN** asynchronous interface preparation exposes that name and type in a prepared CPU schema, after which Main can set it without creating a frame or GPU resource

#### Scenario: Schema re-preparation
- **WHEN** a changed definition produces a new prepared interface
- **THEN** Main explicitly validates and migrates logical values to its new schema/version, with old handles rejected and prior snapshots unchanged

### Requirement: Semantic registry contracts
Reserved Engine and Pbr semantics SHALL define stable names, aliases, types, scope and physical conventions. Custom semantics SHALL use distinct namespaces and reject conflicting registration. Data providers SHALL read immutable Renderer binding contexts rather than mutable Main objects or global current-view pointers. PBR resource semantics SHALL describe roles without forcing all materials to use those roles or texture counts.

#### Scenario: Conflicting semantic registration
- **WHEN** an extension registers a reserved semantic or an existing semantic with a different type or convention
- **THEN** registration fails with the conflicting identity before rendering

#### Scenario: PBR texture roles
- **WHEN** builtin PBR receives base-color, normal and metallic-roughness maps
- **THEN** its schema applies documented color-space, normal-space, UV and sampler conventions while a custom material may use unrelated resource names and layouts

### Requirement: Deterministic parameter source precedence
Each parameter SHALL declare Manual or Semantic source, mandatory/default behavior, and permitted override scopes. Resolution SHALL apply definition defaults, provider values, allowed instance overrides and allowed draw overrides in that order. Locked provider parameters SHALL reject writes through either name or semantic. Clearing an override SHALL restore the appropriate lower-priority source.

#### Scenario: Clearing a manual provider override
- **WHEN** an AllowOverride semantic parameter receives an instance override and that override is subsequently cleared
- **THEN** the next snapshot uses the provider value without dependence on setter ordering

#### Scenario: Missing required input
- **WHEN** a required active provider or manual value/resource has neither a valid source nor permitted explicit default
- **THEN** preparation fails with parameter path, semantic and scope rather than reading uninitialized data

### Requirement: Explicit scope identities and dependency versions
Bindings SHALL support Global, Frame, Scene, View, Pass, Material, Object and Draw scopes with owned context identities and revisions. Derived and mixed-scope data SHALL invalidate on every contributing dependency. Global SHALL be session-scoped. The same frame SHALL support sequential builds for multiple views without value contamination.

#### Scenario: Two views in one frame
- **WHEN** two view identities with different cameras render the same objects in one frame
- **THEN** each view receives its own camera and matrices, object state remains unchanged, and immutable frame values may be shared

#### Scenario: Derived matrix invalidation
- **WHEN** the camera changes while an object's world transform remains constant
- **THEN** a WorldViewProjection parameter and a mixed view/object block are rebuilt for the new view dependency

#### Scenario: Several items from one primitive
- **WHEN** one primitive with one source revision emits multiple items with different World or Draw parameters, including a changed emission order
- **THEN** item/occurrence identities and effective payload content prevent incorrect Object/Draw slice sharing while compatible View and Material slices remain shared

#### Scenario: One frozen view family
- **WHEN** Main submits a material update while BuildViews is preparing a family's views in one Render task
- **THEN** every view uses that family's original primitive/material state, unique pass identities avoid graph name collisions, and the queued update affects a later family

### Requirement: Compatible uniform blocks share GPU storage
The Renderer SHALL provide versioned standard block layouts and arbitrary reflected block packing. Storage reuse SHALL require device, target ABI/layout, logical mapping and effective packed value compatibility. General cache callers SHALL validate complete effective dependency identities/versions and current payload contents. The engine's trusted immutable preparation path MAY reuse unchanged value identities across scope revision changes after validating current dependency ownership and immutable program compatibility. Compatible View and Material blocks SHALL reuse actual GPU buffer slices across draws. Arbitrary mixed cbuffers SHALL be packed as whole blocks without claiming automatic splitting.

#### Scenario: Many objects in one view
- **WHEN** multiple objects use compatible standard View blocks and share a material snapshot
- **THEN** they bind the same View buffer identity and offset and the same compatible Material slice, while independent Object data remains separate

#### Scenario: Equal semantics but different layout
- **WHEN** two shaders store a semantic at different offsets or use incompatible matrix/array layouts
- **THEN** values are packed correctly into distinct compatible-layout storage rather than sharing incompatible bytes

#### Scenario: New scope revision with unchanged block values
- **WHEN** trusted immutable preparation observes a new valid scope revision while a compatible block retains all of its effective value identities
- **THEN** the unchanged block may keep its published immutable slice while blocks with changed effective values are prepared for the new data

### Requirement: Target-aware packing and inactive parameters
Packing SHALL follow actual target member offsets, strides, major order and scalar encodings with initialized padding. Declared parameters optimized out of a variant SHALL remain distinguishable as Inactive; undeclared unknown names SHALL fail. Inactive parameters SHALL not require native resource bindings. Canonical block reuse SHALL validate every active member against its versioned ABI.

#### Scenario: Matrix and array data
- **WHEN** a shader uses nested structs, non-square matrices, booleans and fixed arrays
- **THEN** the packed bytes match the target reflection and the GPU observes the supplied values

#### Scenario: Optimized parameter
- **WHEN** a schema parameter is unused by the selected variant
- **THEN** its logical value can be retained with Inactive status without a fabricated descriptor; a typo not in the schema is rejected

### Requirement: Uniform allocation lifetime and observable reuse
Constant pages and slices SHALL obey backend alignment/range limits and SHALL not overwrite published data while frames or GPU work retain it. Changes to effective packed values SHALL publish new immutable slices unless compatible storage already contains those exact values; unchanged compatible blocks MAY retain existing slices across scope revision changes through trusted immutable preparation. Retirement SHALL progress without another frame and retain only a bounded idle-page reserve. Statistics SHALL expose per-scope evaluation/packing/upload and storage reuse for deterministic tests.

#### Scenario: Update during a gated GPU frame
- **WHEN** changed parameter values are uploaded while an earlier frame's fence is blocked
- **THEN** the earlier frame retains its original slice and bytes until completion, and both values can retire safely

### Requirement: Incremental engine parameter preparation
Renderer SHALL share immutable unchanged inputs and resolved data and SHALL refresh only parameters and constant blocks affected by their effective dependencies. This SHALL apply to arbitrary materials and all supported scopes without application or builtin shader specialization. Frame, Pass and Draw dependencies SHALL NOT alone force complete material re-resolution. Mixed dependencies, overrides, defaults and required inputs SHALL retain existing semantics.

#### Scenario: Shared view over many objects
- **WHEN** a view changes while object and material inputs remain unchanged
- **THEN** view results are shared, unchanged blocks retain their slices without repeated full content lookup, and no per-draw deep copy of the shared view inputs occurs

#### Scenario: Frame and mixed parameters
- **WHEN** a custom shader consumes Frame time, View camera, Object state and a mixed View/Object block
- **THEN** each dependency updates at its proper boundary and changing Frame time does not re-resolve unrelated static parameters or replace unchanged resources

#### Scenario: Default and resource transition
- **WHEN** a provider disappears, returns, or changes a resource while an override is cleared
- **THEN** effective dependencies and values are recalculated correctly, failed preparation preserves old frozen data, and later valid inputs recover

#### Scenario: Draw identity across primitives
- **WHEN** distinct primitives emit draws with identical inputs and the same primitive-local ordinal, with or without stable local item IDs
- **THEN** their Draw scope keys remain distinct so custom providers observing Draw identity cannot be incorrectly reused across them

### Requirement: Bounded parameter cache history
Engine parameter and constant caches SHALL have explicit retention and capacity policies, including a limit on history retained for a long-lived scope. Eviction SHALL release cache references without invalidating externally retained immutable results or submitted GPU work. Statistics SHALL distinguish entries, retained storage, eviction and actual upload.

#### Scenario: Same live scope with changing values
- **WHEN** a direct cache client continually changes effective values under the same valid scope key and lifetime
- **THEN** results remain distinct and correct while cache-owned history stays within its configured budget and released GPU pages can retire

#### Scenario: Old frame during eviction
- **WHEN** cache pressure evicts records whose slices are still referenced by an earlier frame
- **THEN** that frame still renders its original bytes and storage is recycled only after all required CPU/GPU references finish

#### Scenario: Recurring collection exceeds capacity
- **WHEN** a stable ordered collection exceeds the per-primitive evaluation cache capacity
- **THEN** already admitted items retain reuse across frames rather than every item being evicted by the recurring scan, and later working-set changes can replace cold entries

#### Scenario: Default capacity under continuous updates
- **WHEN** changing inputs under live owners continuously require eviction from a saturated provider or constant candidate cache
- **THEN** selecting each eviction victim does not require traversing every resident entry and capacity accounting remains correct across hits, replacements and collection
