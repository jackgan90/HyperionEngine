## Context

At the start of this change, materials carried named semantic declarations and repeated shader member targets. Renderer already owned target reflection, packing, provider scopes and immutable GPU slices. Only four standard blocks used resource-name discovery; PBR separately enumerated environment, shadow and cluster targets. The implemented change now supplies typed identities, shared reflected binding and owner-local contracts. Its declaration syntax still repeats fields and exposes visitor conditionals and manual GPU offsets; the accepted declaration refinement below addresses that remaining authoring burden.

## Goals / Non-Goals

**Goals:** one searchable macro declaration mechanism with central common inputs and owner-local contracts, typed builtin identity, demand-driven versioned resource validation, immutable shared builtin metadata, custom semantic extensions, stable asset compatibility and equivalent rendered behavior.

The declaration ownership refinement below supersedes the original single-catalog placement: only common engine inputs stay in EngineSemantics.inl; the same macro mechanism is reusable by each domain without adding its fields to a global enum.

**Non-Goals:** arbitrary HLSL resource annotations, a new shader language, application/plugin lifecycle changes, new editor commands, replacing generic artist parameter names, removing custom member mappings, archive or Git delivery.

## Decisions

1. Resource names identify engine contracts. Common metadata and owner-local immutable sets record versioned uniform blocks and texture/sampler/structured-buffer resources, including existing shipped names as compatibility identities. Reflection determines physical slots, spaces, visibility and layout. Arbitrary custom resource names keep explicit authoring semantics. Reserved engine contract names fail on incompatible declarations.
2. Generate EEngineSemantic for common inputs and independent domain enums for owner-local inputs. FMaterialSemanticId preserves enum/static descriptor identity without an owned builtin string; custom IDs retain their names. String inputs remain boundary adapters for assets and existing custom authoring. Providers, semantic declarations and runtime lookup use typed IDs. Logical material parameter names remain strings because they are artist-defined and serialized.
3. Each owner's declarations define its value types, scopes, conventions, field order and resource contracts. A shared layout builder derives expected GPU offsets, strides and block sizes using documented packing rules. These declarations are the single source of expected layout and builtin mapping; ordinary fields do not require explicit offsets or a duplicate semantic declaration. Retain individual semantic leaves to preserve existing asset overrides and incremental evaluation, while resource-level contracts group them into complete uniform blocks. Adding a builtin uses the same macro path for every scope.
4. Walk each compiled variant's reflected resources, look up encountered contracts, validate their layout/type, and add only needed semantic bindings. Remove PBR's unrelated feature declarations. Material-authorable PBR values remain predeclared so import and editing before asynchronous reflection still work. Legacy explicit targets remain accepted and checked.
5. Share frozen registries directly. Mutable custom registries are copied and frozen at ownership boundaries so later mutation cannot affect compiled definitions. Builtin tables are shared independently of custom extensions.
6. Fullscreen and compute manual pass parameters remain supported. Their preparation validates encountered contracts through the same validator. Builtin fullscreen values use the typed catalog to translate semantics to reflected targets; no feature-specific string rewriting. Engine automatic providers stay Render-owned; native preparation consumes already frozen values.
7. Preserve target-aware packing rather than copying native C++ structures blindly. Validate kind, scalar, shape, offsets, matrix major/stride, array count/stride and resource count/dimension/comparison/element layout. Extend structured-buffer reflection where the adapter exposes element type information, and reject incomplete information for contracts requiring full validation.
8. Validation applies to resources present in the compiled interface. Inactive reflected members are still checked when present; a wholly optimized-out resource creates no binding. Do not synthesize an assumed layout and claim that absent declaration information was validated. Cache keys include the contract/reflection version.
9. Preserve scope revisions, override rules, immutable snapshots, dependency masks, resource sharing and fence retirement. No high-frequency logs or new RHI/backend special cases.

## Risks / Trade-offs

- Resource names become engine ABI identifiers -> keep versioned shipped names and aliases in each owner's metadata; unknown reserved names produce contextual preparation errors.
- Target packing can differ -> retain native reflection and test DXIL/SPIR-V/MSL preparation and actual D3D12 rendering.
- Structured-resource reflection may expose less information than cbuffers -> verify adapter APIs and distinguish complete layout validation from stride-only checks.
- Existing serialized semantics/targets must still load -> preserve wire fields/names and test round trips and old explicit mappings.
- Generic paths can regress incremental reuse -> run frequency, cache, batching, retained-frame and lifecycle regressions.

## Migration Plan

Introduce typed identities and generated metadata first, then resource discovery and full validation, migrate builtin producers and PBR/fullscreen mappings, add regressions, update docs, validate Debug/Release and style. Keep all source and OpenSpec edits in the current checkout without archive or commit.

## Open Questions

None requiring user input. Adapter details will be resolved against bundled compiler interfaces and recorded if they affect the validation contract.

## Typed parameter structures and submission

The follow-up removes builtin shader variable strings from engine call interfaces. The catalog generates uniform contract IDs, C++ value structures and field-to-semantic metadata. CPU structures contain ordinary typed values; matrix/scalar conversion continues through logical values and native target packing, so CPU object padding is not assumed to be a GPU ABI. Texture, sampler and compute resource inputs use typed semantic IDs. Engine semantic values keep their IDs through publication. Prepared instance overrides and resolved values carry schema-bound handles, including identity and version validation; definition replacement explicitly rebinds through the authored identity boundary.

Shader declarations are generated by engine-owned code from this metadata and injected as immutable virtual shader includes. Compilation cache keys include include names and bytes. This processes shader inputs only: no UHT, C++ parsing, new reflection system, native backend branch or application lifecycle is added. Existing author-written shader layouts remain supported and validated from reflection. HZB copy and reduce use distinct contracts; Deferred lighting keeps a stable declaration across the no-directional permutation. Instance record declarations are generated from the same fields while preserving existing array names, capacity and native layouts.

Builtin fullscreen, local lights, sky, contact shadows, previews, outline, HZB, Triangle and PBR factories use the generated structures/IDs. Runtime fullscreen updates use semantic-to-handle mappings and handle-based clearing. Compute preparation resolves semantics, names and array slots once into program bindings, then submission uses those bindings and preserves graph access, mip ranges, overwrite promises and resource owners. Explicit named custom inputs remain boundary adapters and keep typo/unknown/duplicate rejection.

Semantic group, ownership, authorability and editing hints replace builtin name-prefix or substring policy decisions. Editor preview resolves a builtin semantic to an authored parameter before editing its value, preserving renamed logical parameters. Existing GUI/automation transactions and wire schemas stay unchanged. Padding is validated against the derived expected layout and any explicit fixed-ABI compatibility metadata rather than an arbitrary name suffix. Cache keys retain full canonical layout/mapping/scope/value/resource comparisons; no bare-index-only cache identity is introduced.

## Declaration ownership refinement

Common process/frame/view inputs live in EngineSemantics.inl. Object/draw protocols, material-authorable PBR, shared shadow/cluster/environment lighting and each rendering pass own separate declaration files. Update scope does not determine declaration ownership: a shadow view value remains owned by the shadow contract. Pass-local declarations live with Renderer implementations; reusable CPU material/lighting contracts remain in Materials and never depend on Renderer/RHI.

Each owner generates its own enum and immutable semantic descriptors. Runtime domain identities refer to those static descriptors, so two owners may have the same field names or enum ordinals without sharing identity or allocating name strings. Stable PBR and shared engine wire names remain boundary adapters. Local fields do not extend EEngineSemantic or its common registry.

Generic contract sets carry uniform/resource metadata and generated shader includes. Graphics definitions and compute shader descriptions supply local sets explicitly; common reusable sets are provided by Materials. The binder iterates only reflected resources and looks up contracts in these sets, without including rendering subsystem headers. Generated include names/bytes and complete contract identities participate in prepared-program reuse. Static descriptors and immutable sets survive retained frames.

Selected sets must have unambiguous include and resource names. Repeated identical sets are accepted; conflicting metadata under one include name or duplicate resource identities across different sets fail before compilation. This checks declaration ambiguity, not the ABI of resources absent from reflection. Physical source-root digests still participate in shader cache identity, so the split does not promise that only one subsystem's cache is invalidated after a source edit.

Local numeric shader declarations expose a struct instance (for example DeferredLighting.Viewport and ContactShadow.Viewport), avoiding the global namespace of individual cbuffer fields. Expected layout and C++ values come from the same owner declaration; native reflection and logical target-aware packing remain authoritative. Resource enum IDs keep access, mip ranges, overwrite promises and owner/fence lifetimes. No C++ parser, UHT-like tool, new RHI API or lifecycle branch is introduced.

## Readable declarations and automatic uniform layout

This accepted refinement supersedes the existing author-facing visitor macros and manual offset/size arguments. Ownership remains unchanged: common global/frame/view inputs stay in EngineSemantics.inl, while domain contracts stay beside their owners. Reading an owned shader leads to its corresponding Parameters.inl file, which lists every field once in declaration order.

ContactShadowParameters.inl will use the following frontend:

```cpp
HYP_SHADER_CONTRACT(ContactShadow, 1)

HYP_UNIFORM_BEGIN(ContactV1, ContactShadow)
	HYP_UNIFORM_FIELD(float4x4, ViewProjection, View)
	HYP_UNIFORM_FIELD(float4x4, InverseViewProjection, View)
	HYP_UNIFORM_FIELD(float4, Viewport, View)
	HYP_UNIFORM_FIELD(float3, Eye, View)
	HYP_UNIFORM_FIELD(float3, LightDirection, Scene)
	HYP_UNIFORM_FIELD(float, RayLength, Pass)
	HYP_UNIFORM_FIELD(float, Thickness, Pass)
	HYP_UNIFORM_FIELD(float, Bias, Pass)
	HYP_UNIFORM_FIELD(uint, MaxSteps, Pass)
	HYP_UNIFORM_FIELD(bool, bReversed, Pass)
HYP_UNIFORM_END()

HYP_TEXTURE_2D(float, HierarchicalDepth, Pass)
HYP_TEXTURE_2D(float4, SurfaceNormals, Pass)
HYP_TEXTURE_2D(float4, SurfaceCoverage, Pass)
```

The field arguments are shader type, field name and update scope. Scope affects providers and invalidation, not packing. The uniform arguments identify the versioned resource contract and its shader instance. Contract identity and field order produce typed C++ fields, semantic descriptors, expected layout and HLSL structure fields. Resource declarations follow the same frontend and produce typed resource identities. Authors do not write separate HYP_NUMERIC_SEMANTIC/HYP_UNIFORM_MEMBER entries or visitor-selection #ifdef blocks. Visitor machinery stays inside the shared implementation.

Default policy is supplied by the contract/frontend; only exceptional authorability, editing or compatibility policy needs explicit metadata. Existing wire names and aliases are preserved explicitly when they cannot be derived without changing a shipped identity. Such metadata references a field already declared and does not repeat its type, scope or layout. Independent owners and uniforms retain distinct identities even when their field names match. Per-uniform field enums/traits are permitted; the implementation need not reconstruct a flat domain enum through token concatenation that forces authors to repeat the enclosing uniform name.

The generator continues to expose FContactV1Parameters to C++ and FContactV1Uniform to HLSL. A local shader can state its resource and register directly:

```hlsl
#include "ContactShadowParameters.generated.hlsli"

cbuffer ContactV1 : register(b0)
{
	FContactV1Uniform ContactShadow;
};
```

Shader uses such as ContactShadow.Viewport remain unchanged. Common legacy flat declarations and existing instance-array protocols retain their compatible targets; this refinement does not require converting every shipped block into a struct instance. Generated structures and expected metadata come from the same owner declaration, while actual layout is checked independently through native reflection. The existing virtual-include generation path performs this work; no annotated shader parser, UHT-like tool or additional build executable is introduced.

### Packing and validation

The shared builder computes immutable expected layout once when a contract set is formed, not when frame values are assigned. Ordinary uniform declarations require neither member offsets nor total size. Supported scalar/vector fields follow the documented cbuffer register packing profile: GPU bool occupies four bytes, fields do not cross a 16-byte register, and subsequent fields may share available register components. The default matrix convention is column-major with 16-byte vector stride. Arrays, nested records and alternative matrix conventions must have explicit supported packing rules before they can be exposed by this frontend. Uniform packing and structured-buffer element packing remain separate profiles; a cbuffer rule is not reused blindly for structured resources.

The Contact example preserves the existing layout: ViewProjection at 0, InverseViewProjection at 64, Viewport at 128, Eye at 144, LightDirection at 160, RayLength at 172, Thickness at 176, Bias at 180, MaxSteps at 184 and bReversed at 188, with total size 192. The gap after Eye follows the register boundary rule; authors do not calculate or declare it. C++ sizeof/offsetof, native bool size and C++ object padding do not define GPU layout. Typed logical values continue to be packed using actual reflected target metadata rather than copying an entire C++ object.

Expected layout is derived from declaration rules, not copied from shader reflection and then compared with itself. Each encountered compiled resource still validates scalar type, shape, offset, major order, strides and native extent. Native trailing padding handling remains compatible with the existing validator. DXIL, SPIR-V and MSL are checked separately; automatic packing does not promise identical layouts for arbitrary HLSL types. Unsupported shapes, invalid declarations, arithmetic overflow and target-incompatible reflected layouts fail with contextual diagnostics rather than silently guessing. Absent resources still require no reflected ABI validation.

An exceptional fixed external or historical ABI may require deliberate padding or an offset override. Keep that support in an explicit compatibility mechanism, outside ordinary field declarations. Migration must compare every existing uniform/instance layout and stable semantic/resource identity before removing old offsets; historical gaps must not be silently compacted. A changed contract/reflection representation updates its version and cache identity when needed, while authored identities and compatible layouts remain stable.

### Migration and acceptance

Introduce the readable frontend and shared expected-layout builder, then migrate common, shared-material and pass-local declarations through the same path. Adapt typed C++ generation, contract sets and virtual HLSL includes without changing subsystem ownership. Prefer explicit local cbuffer declarations using the generated type. Preserve legacy wire names, targets, policy and instance capacity through compatibility metadata.

Configure formatting around the stable uniform BEGIN/END macro pair and statement/resource macros as needed, without maintaining a list of per-contract macro names. Formatting must be idempotent and must not indent following top-level shader declarations. Add layout-inference tests against the pre-migration contracts, meaningful unsupported/mismatch cases, scope-independent packing and cross-owner identity tests. Validate native reflection on supported shader formats and affected rendering, then run the required Debug/Release and style/naming/boundary checks. The new tasks remain pending until implementation and verification; previous completed-task evidence remains historical. Do not archive or commit this change.
