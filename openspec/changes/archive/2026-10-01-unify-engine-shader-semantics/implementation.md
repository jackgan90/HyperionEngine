# Implementation and validation

Builtin engine parameters now use generated typed semantic identities. Shader resources select an engine contract by their agreed resource name; reflection supplies the actual register, space, visibility and layout. Common declarations and immutable owner-local sets use one macro mechanism. Preparation validates encountered contracts and derives bindings from their metadata. PBR no longer carries separate environment, shadow or cluster member-name mapping lists. The declaration ownership follow-up below describes the final file placement and domain identities; earlier sections record the preceding implementation phases.

## Delivered implementation

- `Source/Runtime/Materials/Public/Hyperion/Materials/EngineSemantics.inl` is the single catalog for semantic identity, stable wire name, type, scope, convention, resource names, aliases, expected uniform/structured layouts and contract version.
- `EEngineSemantic` and `FMaterialSemanticId` provide enum builtin identity and extensible custom identity. Semantic declarations, providers, runtime input lookup and cache keys use the typed identity. Asset semantic/target fields and artist parameter names retain their existing strings; enum ordinals are not serialized. Engine producers, PBR import and editing use builtin enum references.
- All registries share the immutable builtin metadata. Definitions and providers share already frozen registries; mutable extension registries are copied and frozen at publication. Later registration cannot mutate published contracts.
- Graphics walks actual reflected resources for each variant. Standard uniform validation checks each available field's scalar, shape, offset, size, matrix major/stride and array metadata while preserving native extent. Resources check kind, count, texture dimension, comparison mode and complete structured element fields. Inactive reflected contract fields are checked; absent resources create no engine dependency or validation request.
- DXIL and SPIR-V adapters expose structured element trees. DXIL structured arrays/matrices use structured packing rather than cbuffer strides. Reflection version is 7; shader cache keys include that version. Prepared material keys include the catalog contract version and automatic/explicit binding mode.
- Fullscreen uses explicit mode with frozen values mapped through the same catalog. Compute uses the same validator and offers typed `EngineParameters` for uniform/sampler values, retaining custom `Parameters` and explicit graph texture/buffer access declarations. RHI does not invoke semantic providers.
- Common automatic preparation supplies the existing neutral shadow/cluster/environment defaults when the corresponding resources are discovered. Authored defaults remain authoritative and required Frame inputs remain required. This covers both new automatic bindings and legacy explicit asset declarations.
- `docs/Materials.md` describes the API, compatibility boundary, contract naming, adding semantics, demand-driven validation and fullscreen/compute behavior. Style/boundary tooling recognizes the owned multi-include `.inl` catalog.

## Regression coverage

`EngineSemanticTests.cpp` covers enum/name round trips and aliases, shared frozen metadata, extension snapshot isolation, PBR's material-only predeclarations, reflection-selected registers/spaces, multi-variant resource aliases, explicit mode, cached structured metadata and a wholly optimized-out incompatible resource. Negative shaders cover same-stride wrong scalar and field order, matrix major, array shape, inactive field type, wrong texture dimension and raw/structured kind. Direct resource tests cover comparison sampler mode and descriptor count.

Structured array/non-square-matrix reflection is checked against actual target layouts: the fixture has a 36-byte DXIL element and a 48-byte SPIR-V/MSL element. These layouts remain distinct in reflection. Material asset tests retain wire strings and explicit targets during a typed semantic round trip. Compute rendering publishes typed Frame values, verifies output changes across frames and rejects an incompatible engine block before submission.

The affected rendering suite covers semantic update frequencies, overrides and missing inputs, GPU numeric packing, constant/resource sharing, instancing, retained frames and lifetime retirement, deferred/forward scene rendering, shadows, sky, compute, hierarchical depth, contact shadows and frame recovery. Asset import, publication, automation and editor material paths check persistence/editing compatibility.

The initial rendering regression exposed a lost neutral shadow default: removing PBR's eager feature declarations also removed the old pre-reflection default injection. Shared automatic preparation now applies those defaults after discovery; deferred, transient-material and scene rendering regressions pass with that fix.

## Initial phase verification evidence

Validated on 2026-10-01 with the configured MSVC/Ninja presets. GPU tests ran serially on the D3D12 backend with the debug layer and NVIDIA GeForce RTX 5080. DXIL, SPIR-V and MSL compilation/reflection are covered; native Vulkan/Metal GPU execution was not exercised.

| Check | Result | Local evidence |
| --- | --- | --- |
| Full Debug build, final source | Passed | `out/EngineSemanticsDebugBuildCompleted.log` |
| Full Release build, final source | Passed | `out/EngineSemanticsReleaseBuildCompleted.log` |
| Release affected CTest suite | 32/32 passed, 74.63 s | `out/EngineSemanticsReleaseTestsVerified.log` |
| Debug affected CTest suite | 32/32 passed, 154.41 s | `out/EngineSemanticsDebugTestsVerified.log` |
| Owned paths and formatting | Passed, 970 source files | `python tools/CheckStyle.py` |
| C++ semantic naming/local declarations | Passed, all 40 changed/new translation units | `out/EngineSemanticsNamingVerified.log` |
| Dependency boundaries | Passed, 933 source files / 38 modules | `python tools/CheckBoundaries.py` |
| OpenSpec strict validation | Passed | `openspec validate unify-engine-shader-semantics --strict` |
| Whitespace validation | Passed | `git diff --check` |

The same CTest selector is used for Debug and Release; it selects 30 affected tests and CTest adds two fixture setup tests:

```powershell
ctest --test-dir out/build/<preset> --output-on-failure -R '^(material_(contracts|bindings|rendering|asset_contracts)|compute_rendering|shaders|instance_batching|deferred_rendering|sky_rendering|depth_conventions|cascaded_shadow_maps|cascaded_shadow_rendering|hierarchical_depth|contact_shadows|transient_materials|render_resources|cpu_frame_ownership|cpu_frame_pipeline|model_material_cache|scene_runtime_instance|scene_rendering|d3d12_frame_failure_recovery|dependency_boundaries|code_style_paths|assets_math|async_gltf_import|native_asset_publication|shared_asset_publication|automation_assets|editor_asset_editors)$'
```

The naming check calls `tools/CheckStyle.py`'s `check_naming` with the final Debug compile database and all changed/new `.cpp` files. No checks were disabled. Existing standard-block tests now accept native trailing padding differences while validating every reflected member. Generated root test fixtures were removed; build outputs and detailed logs remain under ignored `out`.

This change remains in `openspec/changes/unify-engine-shader-semantics`. It has not been archived, staged, committed or pushed, as requested.

## Typed parameter structures and submission follow-up

The same active change now extends the catalog to the builtin pass interfaces. `ShaderParameters.h` generates `EEngineUniform`, ordinary typed `F<Name>Parameters` structures, semantic publication and instance-array metadata. Engine pass code assigns fields or supplies resource semantic enums. Logical values continue through native reflected packing; C++ object padding is not copied as a GPU ABI.

- Generated uniform and instance-record declarations are injected as the immutable virtual include `HyperionUniforms.generated.hlsli`. Include names and complete bytes participate in shader cache identity. The shader compiler accepts generic virtual inputs and does not depend on Materials. No C++ scanner, UHT or additional tool executable was introduced. Graphics, compute and GUI compilation supply the declarations at their ownership boundary.
- Deferred directional/local lights, sky, contact shadows and previews, GBuffer debug, tonemap, selection outlines, depth previews and HZB use typed parameter structures/resource identities. HZB copy and reduce have distinct uniform contracts. PBR/Triangle and GUI use shared generated declarations; frame/view/object/draw publication and instance arrays use typed contract IDs. Semantic policies replace builtin name-prefix and editor name-substring decisions.
- Value entries retain semantic IDs or schema-bound handles. Batch submission validates before publishing a snapshot; clearing checks handle ownership. Prepared snapshots and definition transitions explicitly rebind overrides. Schema author identities are interned once into shared weakly owned tokens; cache comparisons still include complete mapping, layout, scope, value and resource identities.
- Strings remain at authored asset/automation interfaces, extensible custom parameter adapters and shader reflection boundaries. Builtin preview resources use `HyperionPreviewSource`, avoiding reservation of a custom shader's generic `Source` texture. Asset parameter/semantic/target wire fields and automation operation schemas remain unchanged.

`ShaderParameterTests.cpp` exercises typed member types, renamed authored PBR parameters, batch failure atomicity, retained snapshots, stale handles, schema rebinding and shared cache identities. It compiles every generated uniform declaration as DXIL, SPIR-V and MSL, validates native reflection, verifies warm cache reuse and virtual-input cache invalidation, and rejects offset mismatches. Existing GPU tests continue to verify generic named custom sampling, image packing, scope frequencies, instance/cache sharing, scene publication and retirement. Test fixtures now compare values after schema rebinding rather than assuming handles are interchangeable between schemas.

The follow-up integration regressions exposed missing generated declarations in GUI compilation and snapshots whose schema was replaced without rebinding handles. Both paths now use the same generated input and explicit rebinding as the other consumers. Custom `Source` sampling exposed the preview resource naming collision; a dedicated engine resource name preserves that authored interface.

### Follow-up verification evidence

Validated on 2026-10-01 with the configured MSVC/Ninja presets. Actual GPU execution used D3D12 with the debug layer; DXIL, SPIR-V and MSL compilation/reflection were exercised. Native Vulkan/Metal GPU execution was not exercised.

The Debug broad run selected 59 tests: 56 passed and three exposed the preview-name collision and test fixtures still treating overrides from different schemas as identical/interchangeable. After the final repairs, all eight focused Debug tests passed, including all three previous failures, contact previews, GUI textures, shader compilation and material contracts/bindings. Release uses the same broad selector and provides 57 tests; scene-dispatch and cache-allocation fault injection are Debug-only and passed in the Debug broad run.

| Check | Result | Local evidence |
| --- | --- | --- |
| Full Debug build, final source | Passed | `out/TypedShaderDebugBuildVerified.log` |
| Full Release build, final source | Passed | `out/TypedShaderReleaseBuildVerified.log` |
| Release affected suite, final source | 57/57 passed, 216.68 s | `out/TypedShaderReleaseTestsVerified.log` |
| Debug focused suite, final source | 8/8 passed, 42.68 s | `out/TypedShaderDebugRepairTests.log` |
| Debug broad integration run | 56/59 passed; all three failures subsequently passed in the focused suite | `out/TypedShaderDebugTestsVerified.log` |
| Owned filenames, include casing and formatting | Passed, 973 source files | `python tools/CheckStyle.py` |
| C++ semantic naming/local declarations | Passed, all 68 changed/new translation units | `out/TypedShaderNamingVerified.log` |
| Dependency boundaries | Passed, 936 source files / 38 modules | `python tools/CheckBoundaries.py` |
| OpenSpec strict validation | Passed | `openspec validate unify-engine-shader-semantics --strict` |
| Builtin string-interface rescan | No builtin named setters or literal uniform/instance contract factory calls remain | `Source/Runtime`, `Source/Plugins` source scan |
| Whitespace validation | Passed | `git diff --check` |

Broad selector:

```powershell
ctest --test-dir out/build/<preset> --output-on-failure -j6 -R 'material_|shaders|scene_|model_|sky_rendering|compute_rendering|hierarchical_depth|contact_shadows|deferred_rendering|selection_outlines|transient_materials|depth_conventions|cascaded_shadow|render_primitives|instance_batching|render_resources|asset_publication|asset_rendering|editor_asset_|automation_assets|automation_capability_parity|automation_parity_regressions|editor_render_|editor_outlines|native_publication_cli|source_fixture_contracts|dependency_boundaries|code_style_paths|cache_allocation_failure|gui_texture_rendering|rhi_backend_contracts|compute_rhi|d3d12_device|cpu_frame_ownership|cpu_frame_pipeline|d3d12_frame_failure_recovery|assets_math|async_gltf_import'
```

Final Debug focused selector:

```powershell
ctest --test-dir out/build/debug --output-on-failure -j6 -R '^(material_contracts|material_bindings|material_rendering|scene_rendering|shared_asset_rendering|contact_shadows|gui_texture_rendering|shaders)$'
```

The source rescan retains intentional authored asset/automation parameter names, named custom compute inputs, reflected native names and compatibility/negative test fixtures. Builtin runtime inputs use structures, enum identities and handles. Generated root test fixtures were removed; logs and build outputs remain under ignored `out`. Tasks for that phase were completed. The change remains active and has not been archived, staged, committed or pushed.

## Declaration ownership and local contracts follow-up

`EngineSemantics.inl` now contains only common process/frame/view inputs and their uniform contracts. Object/Draw and PBR declarations have separate Materials files. Reusable scene lighting, shadow, cluster and environment CPU binding contracts live in Materials/Lighting; producing their values remains Renderer-owned. Deferred/local lighting, ContactShadow, DepthPreview, HierarchicalDepth, Sky, Outline and Output contracts live in Renderer/ShaderParameters. Scope describes update frequency and does not move a domain-specific declaration into the common file.

- `ShaderParameterDeclarations.inl` is the reusable visitor that generates each owner's semantic/uniform enums, ordinary C++ parameter structures and immutable metadata. Domain semantics use static descriptor identity in `FMaterialSemanticId`; local fields never extend `EEngineSemantic`. PBR/shared engine wire names remain compatible boundary adapters.
- Definitions and compute shader descriptions supply `FShaderParameterContractSet` values explicitly. Generic binding and validation use their metadata without importing Renderer subsystem headers into Materials. Reusable standard material contracts are shared separately from each pass's selected sets.
- `HyperionUniforms.generated.hlsli` contains common declarations. Each domain supplies its own generated include. Compilation injects immutable include names and bytes from the same declarations that generate CPU types, with versioned shader cache identity. Graphics, compute and GUI compilation all use this mechanism; there is no persisted production generated file, C++ parser or UHT-like executable. Declaration edits require rebuilding C++ before the next shader compile.
- Local numeric uniforms expose qualified struct instances, such as `DeferredLighting.Viewport` and `ContactShadow.Viewport`. Native reflected wrapper members are normalized into qualified leaf targets and validated against expected layouts. Logical packing retains actual target offsets, matrix stride/major and extent instead of copying C++ struct memory.
- Same-named local fields and equal enum ordinals remain independent. Cache and schema identity preserve complete contract/semantic/layout comparisons. Identical repeated sets are accepted; conflicting metadata under one include name and duplicate resource names across selected sets produce controlled errors. Absent shader resources still cause no ABI validation or parameter dependency.

`ShaderParameterTests.cpp` compiles every common/shared/local generated uniform as DXIL, SPIR-V and MSL, validates reflected layouts, checks generated input/cache changes and rejects real native offset mismatches. Its independent-contract fixture compiles Deferred and Contact uniforms together, verifies same-named fields and distinct handles, publishes independent typed values, checks retained snapshots and verifies wholly absent resources. Negative fixtures reject resource metadata conflicts even when generated uniform bytes match, and resource-name collisions across include names.

The integration run exposed two remaining consumers of the former monolithic include: GUI compilation needed the generated shared Object declarations, and the pending-fullscreen-upload test needed its Output contract and typed values. Both now use the owning contract mechanism; the upload test continues to check one upload, pending readiness and GPU retention rather than shader-name behavior.

### Declaration ownership verification evidence

Validated on 2026-10-01 with the configured MSVC/Ninja presets. Native GPU tests ran serially on D3D12 with the debug layer. DXIL, SPIR-V and MSL compilation/reflection were exercised; native Vulkan/Metal GPU execution was not exercised.

| Check | Result | Local evidence |
| --- | --- | --- |
| Full Debug build and final upload-test rebuild | Passed | `out/LocalShaderDebugBuildFinal.log`, `out/LocalShaderDebugResourceBuild.log` |
| Full Release build and final upload-test rebuild | Passed | `out/LocalShaderReleaseBuildFinal.log`, `out/LocalShaderReleaseResourceBuild.log` |
| Debug affected suite | 58/59 passed, 429.91 s; only the old Output test interface failed | `out/LocalShaderDebugTestsFinal.log` |
| Debug corrected upload test | 1/1 passed, 1.96 s; all selected Debug tests have passing evidence | `out/LocalShaderDebugResourceTestsFinal.log` |
| Release affected suite, final source | 57/57 passed, 204.92 s | `out/LocalShaderReleaseTestsFinal.log` |
| Owned paths/include casing and formatting | Passed, 1003 source files | `out/LocalShaderStyleFinal.log` |
| C++ semantic naming/local declarations | Passed, 70 changed/new translation units plus the newly migrated upload-test unit | `out/LocalShaderNamingFinal.log`, `out/LocalShaderResourceNamingFinal.log` |
| Dependency boundaries | Passed, 966 source files / 38 modules | `out/LocalShaderBoundariesFinal.log` |
| OpenSpec strict validation | Passed | `openspec validate unify-engine-shader-semantics --strict` |
| Whitespace validation | Passed | `git diff --check` |

The broad selector remains the one documented in the preceding phase. Debug-only scene-dispatch and cache-allocation fault injection passed in the Debug broad run. The corrected upload fixture changed only `ResourceTests.cpp`; its Debug and Release targets were rebuilt, its naming check passed, and Debug reran `ctest --test-dir out/build/debug --output-on-failure -R '^render_resources$'`. Release included the corrected fixture in its full affected run.

The final source rescan found no literal builtin setters, uniform lookup/factory arguments or old global domain-enum references in the production paths. Names remain at authored asset/automation/custom shader boundaries, native reflection and declarative ABI metadata. Test-generated shaders and cache files remain under ignored build directories. All 21 tasks are complete; the change stays active without archive, staging, commit or push.

## Acceptance formatting correction

Uniform macro invocations previously omitted a terminating semicolon. clang-format interpreted some following top-level functions/resources as declaration continuations, producing unintended indentation even though its check passed. All 20 uniform invocations in the owned shader sources now end with semicolons, and affected top-level declarations use their intended formatting. The macro's generated cbuffer remains the same; the additional terminator does not change reflected layouts or submitted values.

The complete path/format check passed (`out/UniformReadabilityStyle.log`), as did whitespace validation. The Release affected selector `^(shaders|material_bindings|contact_shadows|hierarchical_depth|selection_outlines|deferred_rendering|gui_texture_rendering)$` passed 7/7 in 19.56 s (`out/UniformReadabilityReleaseTests.log`). This covers shader compilation/reflection and related D3D12 rendering. No C++ source or generated contract metadata changed during this correction. The change remains unarchived and uncommitted.

## Readable declarations and derived layouts follow-up

All 14 common, shared-material and local declaration owners now use the readable frontend. A numeric field is declared once with `HYP_UNIFORM_FIELD(Type, Name, Scope)`; uniform begin/end macros carry only contract and instance names. Resources use texture/sampler/read-buffer macros. Visitor conditionals live in reusable internal defaults/cleanup files. Owner files contain neither the old duplicate semantic/member declarations nor visitor-selection `#ifdef` blocks. Common process/frame/view declarations remain in `EngineSemantics.inl`; rendering-domain declarations remain with their owners.

- The frontend generates ordinary typed C++ parameter structures, a separate field enum for each uniform, domain resource/value enums and immutable descriptors. C++ submission retains typed identities and handles. Same-named fields in different uniforms retain independent identities. Common frame fields now use `EEngineSemantic::Time` and `Index`; stable wire names `Engine.Frame.Time` and `Engine.Frame.Index` and identity order remain unchanged.
- `FShaderParameterLayoutBuilder` derives offsets and total extent from logical field types. Uniform packing uses 16-byte registers, four-byte GPU scalars including bool, and column-major float4x4 matrices with 16-byte matrix stride. Structured elements use their separate tight scalar/vector packing profile. Scope and native C++ object alignment do not participate in either calculation. The builder checks duplicate/empty fields, invalid packing profiles, unsupported arrays/matrices, malformed fixed ABI constraints and overflow.
- The exposed type subset is `float`, `float2/3/4`, `float4x4`, `uint`, `uint2`, `int` and `bool`. Arrays, nested structures and additional matrix forms are rejected until their packing rules and cross-format validation are added. Texture declarations currently support float scalar/vector elements. Structured element layouts are still independently compared to native reflection; tight packing alone is not a cross-backend layout guarantee.
- Exceptional `HYP_UNIFORM_ABI_OFFSET`/`HYP_UNIFORM_ABI_SIZE` metadata remains available for fixed historical gaps/extents. None of the shipped declarations needs those overrides. The migration baseline compares all 21 prior uniforms and their member offsets, sizes, names, types, scopes and policies, plus resource wire names/kinds/strides and aliases. Contact keeps offsets 0, 64, 128, 144, 160, 172, 176, 180, 184, 188 and extent 192 without authored offset arithmetic.
- Generated immutable owner includes expose native HLSL structure types. Local shaders explicitly declare `cbuffer ContactV1 : register(b0) { FContactV1Uniform ContactShadow; };` after including `ContactShadowParameters.generated.hlsli`. Common flat blocks and instance-array protocols retain their existing binding targets. Common generated-declaration version is now 4; include names and bytes still participate in shader cache identity. Generation remains part of C++ contract construction before shader compilation, without a source parser, UHT or new executable.
- Logical typed values are packed against the actual reflected targets. Native layout validation remains independent and demand-driven, and uses the existing matrix/array/resource checks. Semantics absent from a shader cause no ABI validation or dependency. Wire aliases, default input policy, scene ownership and material edit hints remain declared metadata rather than runtime string interfaces.

`ShaderParameterTests.cpp` compares the independent pre-migration baseline, tests register reuse/padding and scope-independent layout, and rejects unsupported/overflow cases. A fixture tests an explicit historical gap/extent and ordinary fields under different scopes. Generated common/shared/local uniforms and the fixtures are compiled and reflected as DXIL, SPIR-V and MSL. Existing cross-owner identity, renamed authored parameters, cache invalidation, stale handles, retained snapshots and mismatched native-layout tests remain active.

Formatting uses a fixed macro set and block rules rather than a growing per-uniform list. Local shader cbuffers and top-level functions/resources have ordinary formatting; all owned sources pass the formatter's idempotence check. The shared Engine enum visitor preserves required macro setup/include/cleanup ordering explicitly. Naming checks retain narrow exceptions for shader language type tokens and generated field enums mirroring boolean member names; ordinary bool declarations remain type-checked.

The material authoring and coding-style documentation describes the frontend, type subset, layout rules and generated include lifecycle.

### Readable declaration verification evidence

Validated on 2026-10-01 with the configured MSVC/Ninja presets. Native GPU execution used D3D12 with the debug layer. DXIL, SPIR-V and MSL compilation/reflection were exercised; native Vulkan/Metal GPU execution was not exercised.

| Check | Result | Local evidence |
| --- | --- | --- |
| Full Debug build and final scope-filter rebuild | Passed | `out/ReadableUniformDebugBuildFinal.log`, `out/ReadableUniformDebugScopeBuildFinal.log` |
| Full Release build and final incremental rebuild | Passed | `out/ReadableUniformReleaseBuildFinal.log`, `out/ReadableUniformReleaseBuildIncrementalFinal.log` |
| Debug affected suite | 59/59 passed, 461.85 s | `out/ReadableUniformDebugTestsFinal.log` |
| Final Debug material/import/publication scope regressions | 7/7 passed, 13.58 s | `out/ReadableUniformDebugScopeTestsFinal.log` |
| Release affected suite | 56/57 passed, 157.25 s; one discovery-capacity failure subsequently passed in isolation | `out/ReadableUniformReleaseTestsFinal.log` |
| Release isolated automation parity regression | 1/1 passed, 5.42 s; every selected Release test has passing evidence | `out/ReadableUniformReleaseDiscoveryTestsFinal.log` |
| Paths, include casing, formatting and formatter idempotence | Passed, 1010 owned source files | `out/ReadableUniformStyleFinal.log` |
| C++ semantic naming/local declarations | Passed, all 71 changed/new translation units; final core changes also passed across 8 units | `out/ReadableUniformNamingFinal.log`, `out/ReadableUniformNamingCoreFinal.log` |
| Dependency boundaries | Passed, 973 source files / 38 modules | `out/ReadableUniformBoundariesFinal.log` |
| OpenSpec strict validation | Passed | `openspec validate unify-engine-shader-semantics --strict` |
| Whitespace validation | Passed | `git diff --check` |

The broad selector is the one recorded in the preceding phases. The Debug broad run completed before the last PBR resource predeclaration adjustment restored the existing Material-scope filter; all current PBR resources have that scope. The final full Debug rebuild and focused selector `^(material_contracts|material_bindings|material_rendering|material_asset_contracts|async_gltf_import|shared_asset_rendering)$` passed, including its model fixture dependency. Release used the final source for its broad run.

The Release automation failure occurred during target discovery, before the model uniform/rendering assertions. Its Editor log showed successful target publication and plugin startup (`out/ReadableUniformReleaseDiscoveryFailure.log`). The current-user discovery directory contained 142 valid records; the failed target was record 132, beyond the existing discovery limit of 128. No automation production code was changed. Rerunning `^automation_parity_regressions$` with `LOCALAPPDATA` temporarily set to the workspace's ignored `out/ReadableUniformDiscoveryIsolation` directory passed; the invoking process restored its original environment afterward. This isolates test discovery without changing or deleting existing system records.

The final declaration rescan found no old numeric/resource/member visitor macros in Source, Content or documentation, and no visitor `#ifdef` or fixed-offset/extent overrides in shipped owner declarations. All 28 tasks are complete. The change remains active without archive, staging, commit or push.

## Independent architecture audit

The audit covers the entire session's tracked and new files relative to `08ac1340b63a6a4849cc07d89ec0c925ad8e586d`, including the typed identity migration, native reflection/contracts, built-in graphics/compute consumers, declaration ownership, readable single-field macros, derived layouts, assets/editor adapters, tools and documentation. The initial frozen manifest contains 164 files and an empty index, with identity `d5d6f0205ae8bdfd66110bf0fc1c4706668199b2da532aff964741f483c038f6` (`out/ShaderArchitectureAudit/Initial/Manifest.json`). Ignored build outputs, dependencies and historical archives are excluded; no unrelated owned-source changes were identified.

Three independent reviewers used GPT-6 Astra at xhigh with no inherited conversation history. Each received the complete requirements, module/lifetime boundaries, compatibility constraints, version identity and existing verification limits, plus a separate contract, runtime or shader/integration focus. Reviewed source remained frozen during the initial review. The main agent independently read every finding's call chain and reran the CPU/D3D12 probes; the repeated runtime name lookup was independently verified from its complete evaluation path rather than a timing estimate.

All eleven confirmed findings, including two found during targeted rereview, are P2 and have bounded fixes within the existing architecture:

| Finding | Trigger and defect | Repair and regression |
| --- | --- | --- |
| C-001 | Predeclared local enum semantic was converted to a string in definition construction, losing descriptor identity | Normalize through the registry without discarding the descriptor; test predeclared Contact enum lookup, single and batch assignment |
| C-002 | A registered custom alias worked with SetSemantic but failed with SetParameters | Normalize batch semantics before handle lookup; test successful alias assignment, canonical/alias duplicate rejection and unchanged retained snapshots |
| C-003 | Scene classified authored names as semantics, treating distinct Pbr.BaseColorTexture and ALBEDO_TEXTURE parameters as duplicates | Keep separate literal-name, semantic and handle duplicate sets; test distinct named overrides and duplicate rejection for every identity form |
| R-001 | Known typed compute inputs optimized out of the shader were rejected, and absent resources still created graph dependencies | Select inputs using compiled native bindings before graph declaration and RHI preparation; test unused numeric/texture/buffer inputs, graph edges, actual GPU execution/readback and unknown/duplicate/access rejection |
| R-002 | Fullscreen treated a Name-only ALBEDO_TEXTURE custom float as a PBR resource semantic and silently discarded it | Optional semantic filtering applies only to explicit typed semantic entries; test the authored short name through fullscreen preparation |
| R-003 | Declared-to-prepared schema bridging repeatedly converted valid handles to names during full evaluation | Add exact authored-token lookup using the existing shared identity tokens; use it in rebinding/provider classification; test index changes, prepared semantic metadata, alias-only renames, type changes and stale handles |
| R-004 | Fullscreen resolved explicit custom semantic aliases before registry normalization | Normalize only the explicit custom semantic branch; preserve authored-name and builtin absence handling; test canonical/alias GPU readback, duplicate rejection and recovery |
| S-001 | Defines and VirtualIncludes could encode the same cache identity despite different compilation semantics | Encode list categories and counts; advance the shader cache key to v12; test independent cold compilation and warm cache behavior across all three formats |
| S-002 | Direct structured matrix elements lost wrapper-block matrix stride/major decorations | Read the native decorations for both nested and direct elements; test direct SRV/UAV matrices in both major modes and cached metadata |
| S-003 | DXIL logical dimensions overwrote differing SPIR-V dimensions, making incompatible structured contracts appear valid | Preserve native shape, with narrowly constrained bool/N-by-1 lowering recovery; test target-dependent float3/float4 rejection and degenerate matrices; advance reflection metadata to v8 |
| S-004 | Bool-to-uint and N-by-1-to-vector lowering recovery blocked each other | Allow the existing bool/uint scalar lowering in the same-width N-by-1 shape recovery; test both major modes, all three formats, independent cold compilation and warm metadata |

Compute declaration reuses CPU compilation under the existing coordinator lock and returns an owned selected description; references into the program cache never escape the lock. Native allocation remains on RHI. Declared-only CPU programs track their scope and are collected even when no material GPU cache was created. Schema bridging adds an immutable per-schema token index, not a cross-frame cache or new ownership layer. Author edits, explicit definition replacement and custom named shader/asset boundaries retain their string adapters and wire formats.

The initial focused Debug run passed material contracts, material bindings, Scene management and compute rendering. A newly added degenerate-matrix test initially assumed the same physical extent for DXIL and SPIR-V; the expected values now respect each native target's packing, and the final shader regression passed. The additional compute graph readback fixture was corrected to describe its initial texture state and clear its output attachment; its final native execution passed. These were new test-fixture corrections and did not require further production changes.

Verification logs, before-fix independent reproductions and fixed-source rereview manifests/reports are kept under `out/ShaderArchitectureAudit`. Broad Debug/Release regressions and original-reviewer rereview are recorded below. Test processes temporarily redirect LOCALAPPDATA to separate ignored per-configuration directories and restore it afterward; the existing system discovery records and production automation behavior are unchanged. Native execution is limited to D3D12 with validation; DXIL/SPIR-V/MSL compilation and reflection are covered, while native Vulkan/Metal execution is not.

The final Release broad run passed 56/57 tests; `editor_asset_editors` and its isolated retry failed at the same main-window Redo assertion. An output-only diagnostic executable, independently reviewed through the input/history/readiness call chain, confirmed that case 201 sent Redo while `Scene.bReady` was false. Every other routing gate was clear, the scene history cursor remained zero, and both scene and asset documents remained clean. Undo triggers asynchronous scene/render preparation; production history shortcuts intentionally reject input before readiness. Both the missing test wait and the production readiness guard are present in the baseline. The acceptance case now waits for readiness before sending Redo, retaining its original dirty-state assertions and global timeout. No production shortcut, history, material or automation behavior was altered. This is an existing acceptance synchronization defect found during validation, separate from the eleven architecture findings. Failure evidence is retained in `ReleaseBroad.log`, `ReleaseAssetEditorsRetry.log` and `EditorDiagnostic.log`; the independent analysis is `ReviewRuntime/SupplementalEditor.md`.

### Audit verification and final disposition

The original reviewers closed C-001 through C-003, R-001 through R-004 and S-001 through S-004. Their final architecture rereview used `ReviewFinalFixes/Manifest.json`, identity `e9c58812a8b14178c04efee6fbb7571ed74a887aab18888b08a35401bf181a8e`, containing 167 files and an empty index. The later test-only synchronization fix used `ReviewAcceptanceFix/Manifest.json`, identity `e4173791c09f2abe82e750b54a10446f5f291d87fc3ff3d44c13c2c1054474ea`, containing 168 files and an empty index. All architecture implementation and regression hashes stayed unchanged during the acceptance repair. Reviewer entry/exit checks found no source drift.

| Check | Result | Evidence under `out/ShaderArchitectureAudit` |
| --- | --- | --- |
| Final full Debug/Release architecture builds | Passed | `DebugRereviewBuild.log`, `ReleaseRereviewBuild.log` |
| Final Debug/Release Editor acceptance rebuilds | Passed | `DebugAcceptanceRebuild.log`, `ReleaseAcceptanceRebuild.log` |
| Debug affected suite | 59/59 passed, 488.02 s | `DebugBroad.log` |
| Final Debug shader/material/compute regressions after S-004/R-004 | 4/4 passed, 13.57 s | `DebugRereviewFocused.log` |
| Debug asset-editor acceptance after synchronization fix | 2/2 including fixture passed, 35.38 s | `DebugAssetEditorsFinal.log` |
| Release affected suite before the test-only synchronization fix | 56/57 passed, 207.43 s; failure independently diagnosed and repaired | `ReleaseBroad.log` |
| Final Release asset-editor acceptance | 2/2 including fixture passed, 13.58 s | `ReleaseAssetEditorsFinal.log` |
| Independent Release asset-editor acceptance | 2/2 including fixture passed, 11.89 s; validation errors 0 | `ReviewRuntime/ReleaseEditorAcceptanceIndependent.log`, `ReviewRuntime/ReleaseEditorAcceptanceLastTest.log` |
| Paths, include casing and formatting | 1010 owned files passed; last changed acceptance file also passed | `RereviewStyle.log`, `AcceptanceStyleNaming.log` |
| Semantic C++ naming for audit repairs | 18 changed units passed, plus the final acceptance unit | `RereviewNaming.log`, `AcceptanceStyleNaming.log` |
| Module dependencies | 973 files / 38 modules passed | `RereviewBoundaries.log` |
| Strict OpenSpec and final whitespace checks | Passed | `DeliveryOpenSpec.log`, `DeliveryWhitespace.log` |

The broad selectors remain recorded in `RunTests.ps1`. Debug's broad run preceded the final narrow S-004/R-004 fixes; the final focused tests cover their shader and actual GPU effects. Release's broad run used all final architecture fixes; only its acceptance synchronization changed afterward, and that original failing test passed in both configurations and in the independent Release run. Every selected Debug and Release test has passing evidence; the original failed runs remain visible rather than being reported as full-suite successes.

The contract reviewer independently rebuilt the original three probes, repeated 18 boundary checks and tested alias atomicity, local-owner identity, exact authored tokens, stale handles, resource/schema/scope lifetime release and persistent wire values. The final type probe confirms that the generic Shaders bool-matrix recovery does not expand the supported Materials/frontend type subset. Evidence and limits are in `ReviewContracts/FinalRereview.md`.

The runtime reviewer independently executed D3D12 preparation/graph probes for optimized-out numeric and texture inputs, zero graph reads for absent resources, strict unknown-input rejection and authored fullscreen names. Canonical/alias actual readbacks, duplicate rejection and recovery passed with zero validation errors. The acceptance fix independently completed the original native-window lifecycle/save assertions. Evidence and limits are in `ReviewRuntime/FinalRereview.md` and `ReviewRuntime/SupplementalEditorRereview.md`.

The shader reviewer independently rebuilt the original cache/structured-matrix/native-width probes, verified 168 pre-existing warm-cache and 168 independent cold-cache scalar/shape combinations, and exercised 10 scalar/width/shape/stride guard cases. All passed. The v12/v8 profile remains valid after S-004 because cached unstripped intermediates are reflected afresh on every cache hit; existing artifacts do not contain serialized faulty reflection. Evidence and limits are in `ReviewIntegration/FinalRereview.md`. Native Vulkan/Metal GPU execution and a runtime performance benchmark were not performed.

No confirmed finding remains open. The baseline contains the acceptance's missing wait, but its exact triggering probability was not compared by executing a baseline build. After the final frozen-source reviews, only this evidence record and the audit task completion marker changed. `Delivery/Manifest.json` records the final bytes and verifies that all code, tests and configuration match the reviewed snapshots. The OpenSpec change remains active; nothing was archived, staged, committed or pushed.
