## Why

The scene renderer emits one GPU draw per render item even when geometry, resource bindings and graphics state are identical. Scene Viewer already repeats shared models, so instance batching can reduce Render/RHI preparation and command recording while retaining independent object state and visibility.

## What Changes

- Add a Render-owned batch coordinator and extensible strategies, initially conventional indexed instancing, with exclusive item ownership and explicit compatibility/fallback diagnostics.
- Compare geometry ranges, effective programs/layouts, graphics state, actual resource bindings and shared constants; permit distinct per-instance numeric material/object/draw values.
- Cache compatible item descriptions and compact visible chunks. Rebuild affected chunks when membership/order or values change; reuse unchanged GPU slices without overwriting in-flight data.
- Add `HYP_ENABLE_INSTANCE=0/1` shader permutations with reflected instance-array contracts, migrate the built-in Model/Triangle/GUI shaders to optional `SV_InstanceID` paths, and preserve ordinary draws through the zero permutation.
- Respect constant-buffer range, alignment, stage binding and shader capacity limits; preserve ordering barriers and whole-model draw failure semantics.
- Enable Scene Viewer batching by default with an A/B switch, diagnostics, deterministic correctness coverage and measured CPU/GPU and frame performance.

## Capabilities

### New Capabilities

- `render-batching`: Generic strategy scheduling, compatibility, exclusive coverage, compact visible caches and failure-safe publication.
- `instance-uniform-rendering`: Reflected instance records, system instance IDs, capability-bounded indexed draws and immutable GPU data lifetime.

### Modified Capabilities

- `scene-viewer`: Batch controls, separate item/draw counts and comparative performance evidence.

## Impact

Materials gains CPU-only authored instance-array declarations; Renderer gains compiled mappings, batch planning, packing and caches; RHI/D3D12 gain instance counts and validation. Built-in shaders, DebugUI's single-draw preparation, Scene Viewer/Viewer diagnostics and relevant tests are updated. Scene stays independent of Renderer/RHI. No new dependencies or native backends are introduced. Static geometry merging, bindless/multidraw, automatic rewriting of arbitrary shaders, transparent reordering and persistent instance-slot indirection are outside this change.
