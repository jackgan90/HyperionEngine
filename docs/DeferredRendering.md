# Deferred rendering and linear HDR

## Frame ownership and selection

Viewer defaults to `FSceneRenderPipeline` with `ESceneRenderPipeline::Deferred`. Main freezes application/material settings; Render declares the complete graph; RHI 0 resolves sources and prepares native resources; the existing frame pipeline records and submits work. Queued graphs retain the resource descriptions/settings they were built with.

`FForwardRenderPipeline` remains the legacy direct-backbuffer API. Viewer `--pipeline forward` instead uses the common linear HDR scene pipeline, making it the reference for Deferred comparisons.

```text
CSM shadow views
 -> BasePass (opaque/masked DefaultLit; four MRTs and D32)
 -> raster fullscreen Lighting (GBuffer + D32 + CSM -> SceneColor)
 -> HDR opaque compatibility (Unlit)
 -> globally sorted HDR transparent geometry
 -> fullscreen Reinhard tonemap -> sRGB backbuffer
 -> explicit legacy display materials
 -> optional GBuffer/depth overlays and GUI
 -> Present
```

HDR Forward replaces BasePass and Lighting with `Forward/HDR`. Both paths share the transparent and output stages. Scene depth returns to DepthWrite before compatibility/transparent draws; simultaneous SRV/DSV binding is unsupported.

ModelViewer supplies the same perspective-camera metadata used by its projection, so CSM works there as well as in SceneViewer. Both viewers expose the shadow controls.

Stable main view IDs are 1 (primary), 2 (compatibility), 3 (transparent) and 4 (legacy display). CSM uses separate reserved IDs and independent BVH queries. `FForwardPipelineStatistics::MainView()` aggregates routed main geometry; per-view rows remain available.

## Color contract

- Base color/emissive texture RGB is sRGB-authored and decoded through the existing mip/import and sRGB SRV path. Alpha remains linear.
- Metallic/roughness, AO, normals, GBuffer data and depth use linear/data encodings. PBR factors, scene vertex colors and light colors are linear.
- Shared lighting writes unclamped linear HDR to RGBA16F SceneColor. Transparent blending happens there before tonemapping.
- The final pass applies positive exposure and component-wise Reinhard, `C = H / (1 + H)`, then writes display-linear RGB to an sRGB RTV. Hardware encodes the transfer function exactly once.
- Existing clear RGB is display-authored sRGB. Renderer decodes it and inverts the tone/exposure transform for the HDR clear, preserving the displayed background. Saturated clears are approximated within one 8-bit code; RGBA16F still has finite range.
- Unlit and transparent materials intentionally change from legacy output because they now share the final tone transform.
- GUI follows tonemapping: authored sRGB RGB is decoded before blending into the sRGB target; font alpha is coverage data. The triangle experiment has an explicit display-color-to-HDR permutation to preserve its existing colors.
- Debug overlays display decoded data. Emissive debug applies Reinhard; depth debug shows raw 0..1 depth. Normal/RMA views are visualization colors.

## Configurable GBuffer

`FGBufferLayout` validates backend capabilities and packing version 1. `shaders/Deferred/GBuffer.hlsli` owns matching encode/decode. Formats can change without changing semantic channel assignments.

| Attachment | Contents | Compact | High precision |
|---|---|---|---|
| 0 | Base RGB, metallic A | RGBA8 UNORM | RGBA16 float |
| 1 | Oct shading normal XY, pre-normal-map normal ZW | RGBA16 float | RGBA16 float |
| 2 | Roughness R, AO G, valid B, reserved A | RGBA8 UNORM | RGBA16 float |
| 3 | Emissive RGB, reserved A | RGBA16 float | RGBA16 float |
| Depth | Window depth in viewport MinDepth..MaxDepth | D32 | D32 |
| SceneColor | Linear HDR RGB | RGBA16 float | RGBA16 float |

Compact GBuffer costs 24 bytes/pixel; high precision costs 32. D32 plus SceneColor adds 12 bytes/pixel. Logical payloads exclude CSM, swapchain/frame depth, allocator alignment and other resources; CSV also records actual device allocations.

C++ clients may override individual entries with supported `EMaterialColorFormat`, including RGBA32 float. Normals and emissive require floating-point storage. Unsupported layouts fail before allocation. A new packing or shading-model protocol requires a new shader contract. The reserved channel is not a material/shading-model ID; future stencil dispatch belongs at the lighting boundary.

Descriptions are immutable. Size/layout/pipeline changes publish complete generations and discard structural view history; old queued/submitted users retain old resources. Stable frames reuse sources, shader programs, bindings/PSOs and scene/instance plans. Shadow allocation scope changes independently.

## Shared shader structure

- `Model.hlsl`: vertex plumbing and macro-selected entry points.
- `Material/Parameters.hlsli`: common shading inputs.
- `Material/ModelMaterial.hlsli`: texture sampling, alpha coverage, sidedness and normal mapping.
- `Lighting/SurfaceLighting.hlsli`: common BRDF, ambient, emissive and CSM evaluation.
- `Deferred/GBuffer.hlsli`: packing and decoding.
- `Deferred/Lighting.hlsl`: exact pixel loads, world reconstruction and raster lighting.
- `Common/Tonemap.hlsl`: exposure/Reinhard output.
- `Common/Fullscreen.hlsl`: reusable oversized triangle VS.

`HYP_DEFERRED_BASE` writes GBuffer without light/shadow bindings; `HYP_FORWARD_HDR` selects shared HDR lighting; `HYP_SHADOW_CASTER` preserves shadow coverage. Existing `HYP_ENABLE_INSTANCE` variants remain supported. Shader contracts cover DXIL, SPIR-V and MSL; runtime acceptance uses D3D12. Reflection version 5 deduplicates system outputs using HLSL's case-insensitive semantic names, preventing SPIR-V/MSL MRT signatures from counting both SV_Target and SV_TARGET.

World position reconstructs through inverse view-projection, viewport origin/extent and depth normalized from viewport MinDepth..MaxDepth back to 0..1. Deferred rejects a zero-width or non-invertible depth range before target allocation or graph mutation. Receiver gradients select continuous depth/normal neighbors; uncovered pixels, silhouette changes and excessive steps use bounded zero correction. Both normals are retained because CSM normal offset uses the pre-normal-map normal. Quantization/reconstruction permit small numeric differences from Forward interpolation.

## Material routes

Built-in definitions expose `HdrForwardOpaque`, `DeferredBase`, `HdrCompatibility` or `HdrTransparent` according to imported alpha/Unlit classification. Lit opaque/masked items contribute once through BasePass, Unlit once through compatibility, and blended items once through globally sorted transparency. Alpha mode, sidedness and Unlit classification are structural definition choices, like blend/depth state; replacing them requires a matching definition.

Custom materials should expose matching HDR usages. A material with only legacy `Forward` renders after tonemapping as an explicit display overlay with its legacy contract. View exclusions keep HDR materials out of that route. Diagnostics report legacy display items; these overlays do not participate in HDR lighting or share scene-depth occlusion.

## Fullscreen and RHI contracts

`MakeFullscreenMaterial(name, pixelShader, bSrgb)` supplies the triangle VS; custom material definitions may provide additional state. `FFullscreenPassDesc` declares reflected parameters, outputs/reads, lifetime and viewport. `AddFullscreenPass` declares on Render and prepares on RHI 0, bypassing primitive collection. Names follow normal reflection, e.g. `Pixel:SceneColor` and `Pixel:OutputV1.Exposure`. Fullscreen draws apply material dynamic blend constants and stencil reference through the ordinary RHI validation path. If a parameter update fails partway through, its cached instance is discarded; a later valid pass starts from declared defaults without retaining partial overrides.

The session caches one indexed oversized triangle: `(-1,-1), (3,-1), (-1,3)`. Lighting, tone, GBuffer debug and depth preview share it, along with standard material binding/constant caches. `bFullTargetViewport` declares whole-attachment output; ordinary viewports remain regional writes.

RHI has ordered color lists, per-slot formats, independent load/store and sampled render-color textures. Full signatures include each attachment effective sRGB format and participate in PSO and retained draw validation. Recording validates dimensions, ownership, formats/count, aliasing and simultaneous sampling/writing. Graph validation tracks every attachment and rejects undefined reads. Texture initialization is queued through existing graphics/fence ownership without a new CPU idle.

## Viewer and measurement

```powershell
out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json
out/build/debug/bin/hyperion_viewer.exe --config experiments/Model.json --pipeline forward
out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json --gbuffer high --gbuffer-debug 2
python tools/MeasureDeferred.py --samples 500 --warmup 300 --repeats 2
```

Saved settings: `render_pipeline` (`deferred`/`forward`), `gbuffer_layout` (`compact`/`high`), `exposure`, `gbuffer_debug` (0..6). CLI overrides: `--pipeline`, `--gbuffer`, `--exposure`, `--gbuffer-debug`. Diagnostics supports live pipeline/layout switching and exposure/debug editing.

GPU timing uses completed submission-aligned timestamps. The serialized runner alternates order, verifies ready/nonempty workloads and exact CPU/GPU correspondence, and rejects failed items, validation errors, descriptor/PSO growth or unbounded allocations. Per-pass total sums recorded GPU intervals; it is not presentation latency. `frame_ms` measures Main tick, `cpu_latency_ms` includes queued work. Pipeline preparation includes Render orchestration, RHI geometry and fullscreen binding preparation. Readback acceptance is separate from timing runs.

See [DeferredPerformance.md](DeferredPerformance.md) for measurements and [OpenSpec tasks](../openspec/changes/archive/2026-09-11-add-deferred-render-pipeline/tasks.md) for delivery evidence.
