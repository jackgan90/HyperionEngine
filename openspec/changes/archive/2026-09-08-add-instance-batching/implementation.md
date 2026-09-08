# Implementation and verification

## Delivered behavior

- Materials/Renderer prepare `HYP_ENABLE_INSTANCE=0/1` permutations. Reflection validates system instance IDs and authored record arrays; unsupported or invalid optional permutations retain ordinary rendering with diagnostics.
- Render owns strategy selection, exclusive source coverage, complete geometry/state/resource compatibility, ordering barriers, bounded description/plan/chunk caches and compact typed instance records.
- RHI 0 creates native packets/slices, reuses immutable cached instance data and preserves whole-model failure publication. D3D12 validates the actual instance count, reflected array shape, compact extent and existing native binding limits.
- Scene Viewer enables batching by default, provides a runtime checkbox and CLI override, and exports actual draw, fallback reason, cache, upload and CPU timing statistics. CLI-forced ordinary mode is explicitly reflected in the GUI.

Interface details and supported limits: [InstanceBatching.md](../../../../docs/InstanceBatching.md). Repeated A/B measurements, CPU/GPU evidence and reproduction: [InstanceBatchPerformance.md](../../../../docs/InstanceBatchPerformance.md).

## Validation scope

The independent audit found and closed three bounded issues: optional native preparation invalidating valid ordinary materials, missing fallback diagnostics, and misleading CLI-forced GUI state. Fixes and independent re-review are recorded in [InstanceBatchAudit.md](../../../../docs/InstanceBatchAudit.md).

The `instance_batching` test exercises DXIL/SPIR-V/MSL reflection and typed packing; real D3D12 image equivalence; optional-permutation rejection and ordinary fallback after native constant-binding or instance-only vertex-input failure; actual count/capacity/short-extent rejection; strategy exclusivity; geometry, state and complete resource binding splits; cache invalidation/budgets; multiview statistics; whole-group failure and late cross-batch repair; and rendering retained old packets after mutation. Native validation errors are zero.

`scene_moving_camera` renders both paths in the same executable, checks exact PNG equality and per-frame visible coverage, verifies actual draws are reduced, and bounds moving-camera chunk rebuild/upload work. Static Scene Viewer image comparison also matched exactly.

All post-audit final checks passed on 2026-09-08:

| Check | Result | Local evidence |
| --- | --- | --- |
| Debug build + full CTest, RenderDoc enabled / Tracy disabled | 46/46, 105.03 s | [DebugVerified.log](../../../../out/InstanceAudit/DebugVerified.log) |
| Release build + full CTest, RenderDoc and Tracy disabled | 41/41, 40.12 s | [ReleaseVerified.log](../../../../out/InstanceAudit/ReleaseVerified.log) |
| Profile build + full CTest, RenderDoc disabled / Tracy enabled | 41/41, 77.82 s | [ProfileVerified.log](../../../../out/InstanceAudit/ProfileVerified.log) |
| Final formatting, owned paths and semantic naming | 268 source files / 166 translation units | [FinalNaming.log](../../../../out/InstanceAudit/FinalNaming.log) |
| Module boundaries | 261 source files / 25 modules | [Boundaries.log](../../../../out/InstanceAudit/Boundaries.log) |
| Strict OpenSpec before archive | 30/30 | [PreArchiveValidation.log](../../../../out/InstanceAudit/PreArchiveValidation.log) |
| Strict OpenSpec after synchronization/archive | 31/31; no active change | [PostArchiveValidation.log](../../../../out/InstanceAudit/PostArchiveValidation.log) |

The final Profile suite includes the real `profiling_trace_acceptance` test (26.20 s), covering reconnects, GPU retirement and capture controls; its [Summary.json](../../../../out/Profiling/Acceptance/20260908-175818/Summary.json) records results. Dedicated earlier performance captures separately cover static/moving × off/on.

The independently reviewed [81-file fixed snapshot](../../../../out/InstanceAuditFixedSnapshot.json) and its [71 source/build/tool file subset](../../../../out/InstanceAudit/VerifiedSourceManifest.json) remain unchanged through final validation. Delivery reports and OpenSpec synchronization/archive follow the re-review. All retained test/performance evidence stays under ignored `out/`.

## Performance conclusion

The pre-audit executable measurement used three serial interleaved runs per configuration, each with 240 warmup and 600 sampled frames: static scene draws 194 → 5 and mean frame time 2.4867 → 1.7029 ms; moving scene draws 194–195 → 5 and mean frame time 2.7650 → 2.1540 ms. Both paths retain identical visible coverage. The gain is CPU preparation/recording; GPU graphics-pass totals and P95 do not show a consistent improvement. See the performance document for raw evidence paths and measurement boundaries.

## Delivery boundary

This change introduces no shader metadata framework, per-instance vertex stream, transparent reordering, persistent instance-slot indirection, static geometry merge or new native backend. All three confirmed audit findings are fixed and independently closed. All 12 tasks and four artifacts are complete. The change was archived on 2026-09-08 after synchronizing 11 requirements into the main specifications. Native Vulkan/Metal, systematic OOM/device-loss injection and post-audit long performance remeasurement remain outside the completed validation scope.
