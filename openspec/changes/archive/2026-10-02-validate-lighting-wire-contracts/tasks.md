## 1. Establish independent baselines

- [x] 1.1 Recheck current structured declarations, CPU records, upload/stride consumers and local-light HLSL; record the exact baseline and coordinate shared shader/CMake/docs ownership.
- [x] 1.2 Add fixed non-symmetric byte/word expectations for point, spot, directional and empty producer data, plus source reuse and old-frame immutability checks; run them on original producers before migration.

## 2. M01A: Validate physical layout and stride

- [x] 2.1 Add narrow native-independent CPU wire validation against existing Materials contract members; derive physical traits from supported actual members and reject unsupported representations.
- [x] 2.2 Associate and validate cluster/directional records once at setup, including standard-layout/trivial-copy constraints and explicit uint2 header/uint index adapters; preserve public record names and fields.
- [x] 2.3 Replace repeated binding strides with validated contract results in default, current and reused-source paths without per-light lookups or altered lifetimes.
- [x] 2.4 Add same-size reordered-field, scalar-kind, offset, extent/stride and unsupported-type negative fixtures, plus fixed 64/8/4/32 expectations.

## 3. M01B: Name shared encoding meaning

- [x] 3.1 Share named CPU local-light range/cone/type encoding between clustered attributes and volume parameters while preserving exact physical components and numerical calculations.
- [x] 3.2 Add named HLSL decoding/fields for clustered and ConeRange inputs; preserve every cutoff, spot threshold, cap and attenuation operation order.
- [x] 3.3 Verify independent point/spot/directional/default word expectations, clustered-versus-volume meaning, primary/disabled/zero directional filtering and identical-byte reuse.

## 4. Validate native data and review

- [x] 4.1 Add D3D12 compute readback of actual producer sources for all structured fields, headers and indices with independent expected values; compile/reflection-check the fixture and affected real consumers for DXIL/SPIR-V/MSL.
- [x] 4.2 Rebuild and run affected Debug/Release spatial/material/shader/deferred/compute regressions; preserve clustered-off volumes, queued source immutability, radiance-only list reuse and zero native validation errors.
- [x] 4.3 Update Materials/lighting documentation and run applicable style/naming, dependency boundaries, diff and strict OpenSpec checks.
- [x] 4.4 Obtain independent frozen-file review, verify and resolve scoped findings, and report M01A and M01B acceptance separately before scoped local commit; do not push.
