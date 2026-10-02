# Implementation and validation

## Migration baseline

The orchestrator built and ran `shader_tests` against the original shader production implementation after adding fixed expectations in `ComputeShaderTests.cpp` and `ShaderReflectionTests.cpp`. Debug CTest `shaders` passed 1/1 in 5.82 seconds. Evidence: `out/maintainability/Orchestration/ShaderBaselineRetryBuild.log` and `ShaderBaselineRetryTests.log`.

The fixtures check b7/t7/s7/u7 bindings 7/1007/2007/3007 in all four spaces, comparison and ordinary samplers, cube textures, readonly/writable structured and raw resources, valid t0/t999/t998[2] ranges, rejected t1000/t999[2]/space4 ranges, and cold/hot bytes, bindings and reflection across DXIL/SPIR-V/MSL. The first test attempt used `SV_PrimitiveID`, which requires a newer MSL version; the fixture was corrected to read its dynamic index from a cbuffer before the passing baseline. Production MSL policy remained 2.0.

## Implementation

- M06: Shaders owns an explicit register-class inventory and checked encode/decode/range helpers, preserving mapping version 2, four spaces and 1000 registers per class. Compiler shifts and SPIR-V family classification consume the protocol; logical DXIL structured/raw reconciliation remains in place.
- M10: Private target values drive the actual DXC argument builder, profile diagnostics and production cache policy identity. The identity frames actual applicable arguments, requested versus payload format, logical DXIL policy, toolchain and existing source/options inputs. Pipeline version is 13; reflection remains 8. MSL 2.0 conversion versions are shared by conversion and MSL identity.
- Private policy tests live within `Runtime/Shaders/Private`, are compiled only into `shader_tests`, and use standard-library checks. No public target override or boundary-check exception was added.

The orchestrator's intermediate Debug build and CTest batch passed 4/4, including `shaders` in 5.96 seconds (`out/maintainability/Orchestration/M09BaselineBuild.log` and `M09BaselineTests.log`). This exercised the production protocol migration and pure private-policy tests. The production argument implementation now resides under `Private/Adapters/ShaderCompileTarget.cpp` to keep DXC flag spellings in the adapter boundary; its value/header and test translation unit remain native-independent.

Additional public-compiler regressions exercise optimization-policy changes and restoration, cold/hot byte and full reflection equality, damaged-cache repair and subsequent hits for all three formats. They inspect cached SPIR-V magic independently and compare it with generated MSL to establish the intermediate-payload contract. The final orchestrator runs below executed these additions successfully.

## Final validation

Before final freeze, all 11 changed/new C++ headers and sources passed the repository's actual-byte formatting check. Owned path checks passed for 1027 sources, dependency boundaries for 990 sources/39 modules, scoped `git diff --check` and strict validation of this OpenSpec change passed. Counts include concurrent changes and are evidence for this run, not repository requirements.

On 2026-10-02 the orchestrator rebuilt shader_tests, material_binding_tests and compute_renderer_tests in Debug and Release on the frozen implementation. The three corresponding tests passed in each configuration: Debug 3/3 in 9.79 seconds, Release 3/3 in 8.28 seconds. Logs are `out/maintainability/Orchestration/M06M10DebugBuild.log`, `M06M10DebugTests.log`, `M06M10ReleaseBuild.log` and `M06M10ReleaseTests.log`. Concurrent M09 CPU option work was held at a compile-complete boundary during these builds; this does not claim final M09 acceptance.

Final actual-byte formatting, all owned filename/include casing, eight changed translation units' semantic naming, dependency boundaries, scoped diff and strict OpenSpec validation passed. Evidence: `M06M10StyleNaming.log` and the orchestrator validation record. Independent review read all 20 frozen files and verified their hashes before and after; no confirmed finding, supported unresolved suspicion or repair request remained (`M06M10CodeReview.md`). Root also directly checked production key/argument/reflection paths and independent literal test expectations. Only this result record and task completion marks changed after source review.

M06 acceptance covers the stable explicit mapping ABI, checked bounds and real cross-target fixtures. Non-DXIL invalid-range end-to-end cases can be rejected by their logical DXIL prerequisite; pure decoder negatives and real SPIR-V positives separately cover the migrated decoder. M10 acceptance covers actual applicable policy/key/diagnostic identity and cold/hot/restored/corrupt-cache behavior. Native runtime evidence is D3D12; SPIR-V/MSL evidence is compilation/reflection and MSL text generation, not native Vulkan/Metal execution.
