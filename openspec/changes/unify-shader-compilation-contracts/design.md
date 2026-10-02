## Context

M06/M10 were confirmed at `4da30568bb09e4f8ec60be596df7f822f77724b1`; see `out/maintainability/Orchestration/ShaderRhiInvestigation.md`. Compiler arguments encode four register classes with ordinal shifts, while SPIR-V reflection repeats 1000/2000/3000. Cache identity already includes sources, toolchain, options, generated includes, mapping and reflection versions; the gap is that actual profile/HLSL/environment policy is repeated separately and depends on a manual salt.

Shaders owns both contracts. Resource kind differs from register class: several SRV/UAV resource kinds share t/u, and SPIRV-Cross storage buffers include both readonly and writable resources. MSL is generated from cached SPIR-V, not a cached final MSL string.

## Goals / Non-Goals

**Goals:** One checked register protocol; one actual target description used by arguments, diagnostics and cache key; unchanged supported artifacts and normalized reflection; fixed independent compatibility tests and policy-sensitive key tests.

**Non-Goals:** New backend/profile/space support, public target-policy configuration, cache eviction/remote cache, merging SPIR-V/MSL cache entries, changing reflection wire data or asset identities, and unrelated material/renderer cleanup.

## Decisions

### Register classes are explicit, independent of resource enum ordinals

Add a small Shaders-owned `EShaderRegisterClass` with explicit b/t/s/u identities and checked class/space/register/count helpers. Existing public limit constants remain available and refer to the same protocol: four spaces, 1000 registers per class, mapping version 2. Encode/decode use this definition; native DXC shift flag spellings remain in its private adapter. Unknown class, binding outside the four intervals, space >=4, zero/unbounded count or a range crossing its class are rejected before arithmetic overflow.

Compiler shift arguments iterate an explicit class inventory. Reflection decodes class first, checks compatibility with its resource family, and preserves the existing DXIL logical-reflection reconciliation for raw/structured and other logical information. DXIL and SPIR-V range validation share the same bounds. No resource kind ordinal becomes part of the encoding.

Moving only numeric literals to a header would leave the class/range rules duplicated. A generic register allocator or resource registry is unnecessary; this change only expresses the existing protocol.

### Describe applicable compilation policy as a private value

Create a focused private, native-independent target value with stage profile, HLSL language version, strictness, optimization and, for SPIR-V payloads, target environment and reflection/mapping inputs. Keep default profiles vs_6_0/ps_6_0/cs_6_0, HLSL 2021 and Vulkan 1.1. The value distinguishes requested final format from DXC payload format; MSL's payload is SPIR-V.

One adapter builds the exact target-specific DXC arguments from this value. Diagnostics read its selected profile and payload target. Cache identity includes its semantic fields using existing unambiguous length framing, while retaining source-root digests, logical paths, entry, normalized defines/virtual includes, toolchain identity and reflection/mapping versions. Explicit pipeline version remains for conversion or processing changes that the value cannot represent. A one-time change to key framing/version is acceptable; no user cache deletion is required.

Only applicable fields affect each target. Changing an unused Vulkan environment must not invalidate DXIL. Labels/diagnostic wording never enter identity. For non-DXIL targets, include or deterministically derive the DXIL logical-reflection policy as well: changing a policy that affects logical metadata must not leave a stale artifact identity. Preserve the same immutable source snapshot for both payload and logical compilation.

MSL generation and its existing version salt remain explicit. If conversion options move into the value, apply them to the converter and the appropriate requested-format identity together; do not describe an unused setting as a cache input or pretend the cached payload is final MSL.

A public override API would expand supported behavior solely for testing. Instead test private pure-value policy builders through a narrowly scoped private test include, permitted for tests, or compile a focused adapter test with the existing test target. Production compilation and key construction must call the same tested functions; testing an unrelated synthetic key builder is insufficient.

### Compatibility, failure and lifetime boundaries

Do not change the normalized reflection version or mapping version for equivalent encoding. Keep rejection of unsupported ranges, damaged-cache recovery, cold/hot reflection reconstruction and deterministic normalized options. Cache hits do not replay compiler warnings. The compiler still owns native adapters, captured sources and include-handler inputs; no global mutable policy or borrowed request state is introduced.

M09's planned generated Visualizer defines already use the existing defines input. This change preserves that input and does not depend on or modify RasterOptions/Renderer. Sequence shared shader test/CMake/doc edits through the orchestrator.

### Separate acceptance evidence for M06 and M10

M06: fixed samples b7=7, t7=1007, s7=2007, u7=3007; all four spaces/classes; register 0/999 and arrays ending at 1000; reject invalid classes, spaces, count zero, overflow and boundary crossing. Actual fixtures cover constant buffers, sampled/comparison samplers, texture/cube, structured/raw and writable resources, not round-trip alone.

M10: mutate only profile, language version or applicable environment in the private target and assert both production argument output and production identity change. Assert irrelevant environment/labels do not affect DXIL. Restore identical policy and exercise actual cold/hot compilation. Retain define, virtual-include, source/include, toolchain and version inputs; compare artifact bytes, bindings and reflection across cache hits and corrupted-cache recovery. Use independent expected arguments/mapping values rather than the same encoder as oracle.

Build and run `shader_tests`, `material_binding_tests` and `compute_renderer_tests` in Debug/Release; CTest `shaders|material_bindings|compute_rendering`. Run formatting, changed-TU naming, module boundaries, diff check and strict OpenSpec validation. Native execution evidence is D3D12 only; SPIR-V/MSL validation is compilation/reflection. Independent reviewer examines final files before completion.

## Risks / Trade-offs

- [SRV/UAV storage-buffer misclassification] → Validate decoded class against reflection family and retain logical DXIL reconciliation plus fixtures for raw/structured resources.
- [Only the helper test changes while production key ignores policy] → Route real compile/key/diagnostic paths through the tested value; inspect call sites independently.
- [Overflow or array tail escapes class] → Validate by subtraction after bounds, test maximal integer inputs.
- [Unnecessary cache misses] → Exclude unused target fields and presentation; retain requested-format cache separation without new optimization.
- [Lost old inputs or hot-path reflection] → Keep existing cache/source tests and cold/hot artifact comparison, including MSL regeneration.

## Migration Plan

Record existing shader regression results, add fixed protocol evidence, introduce and migrate register rules, then introduce the target value and key/diagnostic/argument consumers. Keep M06/M10 task groups separate. Rebuild, validate, independently review and update Materials documentation before a scoped local commit. Source rollback is sufficient; existing assets remain compatible and prior cached payloads remain harmless under their old identities.

## Open Questions

None blocking. Helper placement/names and private test access are implementation choices within the ownership and validation constraints above.
