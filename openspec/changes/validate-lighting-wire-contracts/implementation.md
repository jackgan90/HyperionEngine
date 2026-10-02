# Lighting wire implementation evidence

## Original-producer baseline

The original producers were frozen at repository commit `4b9caf537795ac46c597f812fa46d6f404a1c927`, before production changes. The baseline manifest and preserved source copies are recorded by the orchestrator under `out/maintainability/Orchestration/M01BaselineFiles.json` and `M01Baseline/`.

- `scene_spatial_visibility` passed the fixed little-endian point/spot/directional/default words, header/index values, source reuse and retained old-frame bytes.
- `compute_rendering` passed D3D12 field reads from actual producer sources: all four lighting resources, every field/component and GetDimensions metadata. Its independent output expectations remain 152 nonempty words and 35 default words.
- `deferred_rendering` passed the existing lighting regressions.
- The initial `shaders` run exposed a baseline-test expectation error: `HYP_NO_DIRECTIONAL` in `Deferred/ClusteredOnly.hlsl` removes primary directional lighting, while `Deferred/Lighting.hlsl` still evaluates additional directional lights. The strict expected resource mask was corrected from 7 to 15; all physical member and stride checks remain. Source/target/actual/expected diagnostics were added. The orchestrator rebuilt and reran `shaders`, which passed DXIL/SPIR-V/MSL fixture and real-consumer checks.

Logs: `out/maintainability/Orchestration/M01BaselineBuild.log`, `M01BaselineTests.log` and `M01ShaderRetryM11BBaselineTests.log`. These original-producer checks are a separate baseline from the final production validation recorded below. Cross-target compilation/reflection does not imply native execution beyond D3D12.

## M01A checkpoint

The physical validator is implemented in Materials, with actual C++ member descriptors tied to their owning record type. Supported physical storage is finite; logical bool/matrix/array conversions do not opt types into raw upload. Renderer associates its existing public cluster records and private directional record to the existing typed owner contracts once, validates before source creation and caches their strides. The uint2 header and anonymous scalar index use explicit adapters. Same-size reordered records, scalar/shape mismatches, padded/reordered headers, added extent, unsupported physical types and ineligible C++ records have focused rejection fixtures.

The orchestrator verified all 22 M01A frozen file hashes, built Debug `material_binding_tests` and `spatial_tests`, and ran `material_bindings` plus `scene_spatial_visibility`: both passed. Evidence is in `out/maintainability/Orchestration/M01ABuild.log`, `M01ATests.log` and the accepted `M01AFiles.json` manifest. This accepts the physical-layout checkpoint, including the actual alternate-type rejection fixtures and unchanged CPU producer words. At this checkpoint, Release, final native/reflection regressions and independent review were still pending; their final results are recorded below.

## M01B checkpoint

`LocalLightEncoding.h` now produces named inverse range, inner/outer cosine and the exact 0/1 spot flag. The cluster producer uses its cluster-record encoder; the actual volume material's `ConeRange` setter uses its volume encoder. Both preserve the original `1.f / Range` and field values. A focused private test translation unit compares each representation to independent point words `3e000000 3f600000 3ec00000 00000000` and spot words `3d800000 3f400000 3e800000 3f800000`, without deriving one representation's expectations from the other.

HLSL decodes the cluster record and existing volume float4 into named attenuation fields. The float4 `EvaluateLocalLight` entry remains as a decoding wrapper. The evaluation retains its original expressions, range cutoff `>= 1`, distance threshold `> 1e-12`, distance floor `.0001`, spot threshold `> .5`, outer-cone cutoff `<=`, smoothstep bounds, radiance cap `1e20` and final cap 65000. Physical HLSL records and all fixed original producer/readback/reflection expectations remain unchanged.

M01B source, HLSL, CMake and naming configuration were frozen for the orchestrator's shared build in `out/maintainability/Orchestration/M01BSourceHoldFiles.json`. This freeze preceded final validation. The completed acceptance results follow.

## Final acceptance

Both M01A and M01B are accepted. All affected targets built successfully in Debug and Release. The seven M01 tests passed in each configuration: `material_bindings`, `material_rendering`, `scene_spatial_visibility`, `shaders`, `compute_rendering`, `deferred_rendering` and `compute_rhi`. Debug results are the M01 entries in the combined `M01M11BDebugTests.log`; unrelated M11B negative-test failures in that log are not reported as passes. Release results are in `M01FinalReleaseTests.log` (7/7). The original producer oracle files retain their accepted baseline hashes.

Source formatting, changed-TU semantic naming, owned paths, dependency boundaries, scoped diff and strict OpenSpec checks passed. The independent reviewer checked all 32 frozen paths, their actual implementation and supporting calls, and rechecked every hash; `M01CodeReview.md` reports no confirmed defect or unresolved suspicion. Root separately inspected the layout, producer, encoding and shader changes. The final task/evidence update records these results without changing reviewed source.

The acceptance is bounded to native D3D12 execution and DXIL/SPIR-V/MSL compilation/reflection. It adds no Vulkan/Metal native execution claim. Full orchestration receipts and exact file hashes remain under `out/maintainability/Orchestration/`; this change is retained active for a separate archival step.
