## Context

Materials `ClusterParameters.inl` and `SceneLightingParameters.inl` already define the expected structured layouts, independently of native reflection. Renderer uploads `FClusterLightData`, `FClusterHeader`, uint indices and a local directional-light record by copying their object bytes. Current checks cover 64/8/32 total sizes; binding sites repeat 64/8/4. ClusteredLights and LightVolumePass separately encode inverse range, inner/outer cosine and the spot flag, and HLSL reconstructs their relationship with component indices.

These patterns create maintenance hazards; they are not evidence of a current visible lighting failure. Keep current layouts and numerical behavior. Native upload validation complements existing shader-reflection validation: neither is an oracle for the other.

## Goals / Non-Goals

**Goals:** Prove CPU physical layout against the existing contract, derive binding strides, express local-light encoding meaning once per language and independently test physical values and ownership.

**Non-Goals:** Generating all C++/HLSL structs, arbitrary struct serialization, matrix/array/bool direct-upload support, a new schema or resource registry, light algorithm changes, new backends, new render options and per-frame diagnostic logging.

## Decisions

### Existing Materials declarations remain the layout authority

Add focused, native-independent validation support in Materials for explicitly supported physical wire types. Compare member name, scalar kind, columns/rows, byte offset and extent, plus total stride. Require standard-layout and trivially-copyable CPU records. A typed member helper derives physical type/extent from the actual C++ member; use standard `offsetof` for the offset and avoid pointer-representation tricks. A small macro or declaration helper may bind the member token, its name and offsetof together so authors cannot independently provide a different same-type member name.

Keep the supported physical set narrow: current float4 records and uint scalar/vector storage. Do not reuse logical `TShaderValueType<Bool>` as four-byte wire storage; C++ bool is not a supported upload representation. Matrices, arrays, unknown types or incompatible packing reject explicitly. The expected GPU layout must still come from the existing declaration builder, never from C++ sizeof/offsetof or native reflection.

Renderer owns the concrete upload records and their association to the relevant typed resource semantic. Validate once at immutable contract/producer setup and cache the validated result. Per-light encoding must not look up strings, rebuild layouts or allocate descriptors. Binding sites consume the validated contract's stride rather than repeated numeric literals.

`FClusterHeader` is a named Offset/Count CPU pair while the shader contract is one unnamed uint2. Preserve that readable public record and explicitly validate its two uint32 members at offsets 0/4, contiguous eight-byte storage, and the contract's single unnamed uint2 shape. The uint index adapter validates an unnamed uint scalar. These focused aggregate adapters must not falsely claim that a C++ struct is automatically any same-sized shader vector. A broad flattening/serialization framework is unnecessary.

### Keep the finite wire records and independent shader structs

Preserve the 64-byte cluster record (PositionRange, RadianceType, DirectionInner, Outer), eight-byte header, four-byte index and 32-byte directional record. Move the local directional wire record to a focused Renderer private declaration if reuse/testability requires it. Keep public cluster type names and field access compatible.

HLSL keeps its explicit records and continues to be validated by the existing compiled-reflection path for DXIL, SPIR-V and MSL. A target with incompatible reflected layout fails the existing contract boundary; validating a CPU record does not declare arbitrary target layouts upload-compatible. No contract version bump is needed for equivalent bytes.

### Name local-light physical meaning in both languages

Renderer provides one focused local-light encoding helper for inverse range, inner cosine, outer cosine and spot classification. Clustered attributes and volume ConeRange both consume this value through named encoding functions. Position/radiance/direction fields remain explicit, and the spot flag remains exactly 0 or 1. Preserve inverse-range calculation, clamping/attenuation order and existing input validation ownership.

HLSL introduces named decoding/accessors for the same attenuation/cone meaning. Clustered records decode through one helper, and LocalLighting consumes named fields rather than scattering x/y/z/w meanings. Light-volume shader input continues to use its existing ConeRange layout. Preserve the `> .5` spot test, range cutoff, smoothstep bounds, distance floor, radiance cap and final 65000 cap exactly; do not change BRDF formulas or float operation order as cleanup.

The layout adapter and encoder solve different problems. Typed field offsets cannot prove that InnerCos was assigned to the correct physical slot. Independent numerical expectations are required for both checkpoints.

### Preserve publication, reuse and selection behavior

Do not change cluster assignment keys, budgets or conservative coverage. Radiance-only edits update attributes without rebuilding lists, unchanged header/index/light bytes reuse their sources, and previously published frames retain immutable bytes through reset/replacement. Default empty buffers remain one zero cluster/header/index record. Directional upload continues to exclude the selected primary light, skip disabled/nonpositive radiance, preserve order and use its existing empty `(0,0,1,0)` direction with zero radiance. Identical directional bytes reuse the previous source.

No new GPU waits, cache invalidations, mutable shared wire records or logging in the light/frame path. Existing resource scope/fence ownership remains with Renderer/RHI.

### Independent acceptance evidence

1. Before replacing producers, add fixed expected little-endian words/bytes with deliberately different position, radiance, direction, range and cosine components. Assert point, spot, directional and default bytes without memcpy into the same record type as the sole oracle.
2. Validate fixed expected layouts 64/8/4/32. Deliberately exchange two float4 fields while total size remains 64, change a scalar kind, offset or stride, and verify rejection. Exercise the header pair and uint adapter separately. Verify unsupported bool/matrix/array paths cannot silently pass.
3. Use actual producer sources in a focused D3D12 compute fixture that reads every structured field and writes a flat buffer; compare against independent literal/numerical expectations. Include header offset/count and index data. Native helper tests that only copy the original CPU bytes do not replace this evidence.
4. Compile/reflection-check the real lighting consumers and readback fixture for DXIL/SPIR-V/MSL. Check fixed known member offsets/strides where supported; report cross-target compilation separately from D3D12 native execution.
5. Compare clustered and volume shading/encoded meaning for point/spot, including cutoff/cone boundaries; preserve existing Deferred/Forward, clustered-off volume and directional regressions. Retain source identity, old-frame byte immutability, radiance-only no-list-rebuild and empty/reset tests.

Build/run Debug and Release using affected existing targets such as spatial_tests, material_binding_tests, shader_tests, material_gpu_tests, deferred_render_tests and compute_rhi_tests; select the actual final fixture target after reviewing direct dependencies. Serialize shared CMake/Materials documentation and GPU tests with other changes. Run style/naming/boundaries and strict OpenSpec validation, then independent frozen-file review.

## Risks / Trade-offs

- [Same-size wrong-order or wrong-semantic data] → Typed member/offset checks plus independent non-symmetric byte and GPU expectations.
- [A physical validator accepts logical bool or target-dependent matrices] → Deliberately limited physical types and explicit rejection; no inferred generic uploader.
- [Header pair adapter loses meaning] → Explicit Offset/Count member/type/offset checks and fixed GPU component expectations.
- [Refactoring changes attenuation] → Preserve operation order/constants and compare named packing with independent expected words and existing rendering regressions.
- [Convenient record regeneration changes public ABI or lifetime] → Keep existing public records and resource publication/retirement, validate their real storage instead.

## Migration Plan

Freeze byte/behavior evidence, add supported wire validation and migrate stride consumers, then introduce shared named encoding/decoding. Validate M01A layout and M01B meaning separately, run affected final regressions and review, update documentation and perform one scoped local commit. No asset/cache format migration is required; source rollback restores the prior implementation.

## Open Questions

None blocking. Exact helper names and the test-only private entry for directional/volume data are implementation details; prefer a same-module test translation unit over exposing private objects or weakening module boundaries.
