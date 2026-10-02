## 1. Confirm the implementation boundary

- [x] 1.1 Read the design, current shader compiler/reflection consumers and existing shader tests; record the actual baseline and coordinate shader test/CMake/doc ownership with the orchestrator.
- [x] 1.2 Run existing shader regression before migration and add independent fixed mapping/resource fixtures where they can exercise the original path.

## 2. M06: Shared register protocol

- [x] 2.1 Add typed register classes and checked encoding/decoding/range helpers with existing public constants and values preserved.
- [x] 2.2 Migrate compiler shifts, SPIR-V class decoding and shared DXIL/SPIR-V resource bounds; preserve logical raw/structured reconciliation and supported arrays.
- [x] 2.3 Verify fixed b/t/s/u samples, every supported space, first/last registers, array tails and invalid/overflow cases; compile actual resource fixtures for DXIL/SPIR-V/MSL.

## 3. M10: Actual compilation policy and identity

- [x] 3.1 Introduce the private target description and route production DXC target arguments and profile diagnostics through it, preserving existing default policy.
- [x] 3.2 Include applicable payload/logical-reflection policy in the production key while retaining existing sources, defines, virtual includes, toolchain and explicit mapping/reflection/pipeline versions.
- [x] 3.3 Add controlled private-policy tests proving profile/language/environment changes affect real arguments and key, while inapplicable fields/presentation do not; no public user override API.
- [x] 3.4 Verify actual cold/hot artifact bytes/reflection equality, stable-policy hits, restored-policy reuse, corrupted-cache recovery and MSL regeneration; preserve all existing cache input regressions.

## 4. Validate and review

- [x] 4.1 Build shader_tests, material_binding_tests and compute_renderer_tests in Debug/Release and run shaders, material_bindings and compute_rendering; record native D3D12 versus cross-target compile/reflection limits.
- [x] 4.2 Update Materials documentation; run formatting, changed-TU semantic naming, dependency boundaries, diff check and strict OpenSpec validation on the final files.
- [x] 4.3 Obtain independent final review, verify findings directly, apply confirmed scoped repairs and repeat affected checks; report M06 and M10 acceptance separately to the orchestrator.
