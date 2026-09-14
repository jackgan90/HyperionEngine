## Context

The accepted exploration identified graphics-only shader stages, RHI pipeline descriptors and graph passes. The current D3D12 ShaderRead state already includes pixel/non-pixel reads, but resource bindings have no UAV views. Rendered textures are single-mip, graph hazards are texture-wide, and typed buffers use upload heaps. Each compiled graphics segment consumes one of 16 recording contexts; timing queries use the same indices.

FSceneRenderPipeline inserts Deferred lighting immediately after the GBuffer BasePass. Compatibility, sky and transparency follow lighting. GBuffer already contains geometric normals and surface coverage; reconstruction handles the viewport depth range. SceneViewer uses experiments/Scene.json and the mounted /Game/Scenes/Sponza.hasset.

## Goals / Non-Goals

**Goals:** independently usable compute shader/pipeline/parameter/dispatch APIs; safe mixed graphics/compute execution; requested reusable HZB; short directional contact shadows; dynamic DebugUI controls; dual depth convention and Sponza validation.

**Non-Goals:** async compute queues, GPU occlusion culling or SSR consumers, Forward depth prepass, contact shadows for later compatibility/transparent surfaces, local-light contact shadows, temporal accumulation, Git commit.

## Decisions

### Compute program and publication

Add Compute to portable shader stages and reflect fixed thread-group dimensions and writable texture/structured/raw resource kinds on DXIL/SPIR-V/MSL paths. Change reflection/cache version when the contract changes. Existing source/entry/defines and mounted-source semantics remain shared. Only D3D12 runtime execution is promised.

A compute pipeline contains one compute artifact and binding layout, with explicit kind validation; it carries no vertex or graphics-target state. A dispatch owns its pipeline, immutable bindings, constant slices and three-dimensional group counts. RHI capabilities describe usable compute and storage resources, including group/resource limits. Compute uses the current direct queue, preserving submission order without additional CPU/GPU waits.

Renderer exposes a compute program and pass descriptor using named reflected parameters and engine-owned resources. Values are owned/frozen at graph declaration. Common reflection layout and constant packing helpers are shared with graphics, while compute compilation and execution remain independent of a material's mesh/VS/PS. Stable layouts/PSOs/resource views are cached; numeric revisions do not rebuild them. Resource identity includes mip and buffer range/stride. Native creation/retirement stays on RHI 0 and recorded packets retain all submitted generations.

### Resource and graph contracts

Introduce sampled/storage texture views with explicit mip ranges, writable structured/raw buffer views and GPU storage buffer allocation. R32Float is required for HZB; a single-channel normalized mask format is preferred with explicit capability validation. Graphics can sample compute-produced resources through the same native identities. Readback diagnostics retain float precision.

Graph access declarations precede native preparation. Compute pass preparation only materializes owned dispatches and cannot change topology/accesses. Texture hazards and initialization are tracked per mip, buffer accesses conservatively per physical buffer, and sampled/UAV reads/writes are validated against the actual bindings. Non-overlapping mip reads and writes are legal; overlapping incompatible SRV/UAV accesses are rejected. A UAV write declares full overwrite or preservation requirements; issuing any dispatch alone is not proof of full initialization. Empty work cannot satisfy a promised write.

Generate subresource transitions and UAV barriers for dependent accesses that remain in UAV state. Preserve initial/export state and content rules, imports with stable physical identity, RAW/WAR/WAW ordering, and negative validation before GPU work.

Separate ordered logical passes from bounded command-list recording batches. Batches contain contiguous pass sequences and submit in graph order. Keep independent per-pass profiling/query indices, graphics state boundaries, immutable retention and cancellation/failed-Present recovery. This replaces the accidental 16-pass limit without unbounded eager allocator creation or a larger hard-coded context count.

### Requested hierarchical depth

An HZB producer accepts requests keyed by graph/frame, view identity and camera/depth convention, source generation, dimensions/viewport, and reduction requirements. Compatible consumers share one declared product. The first request declares production; no request means no generation or new active product allocation. Requests do not persist as stale consumer counts. Debug preview is an explicit consumer. The producer never reads contact-shadow configuration.

Use an R32Float texture with a real mip chain. Initialize mip zero from current depth, normalized for viewport depth range; reduce each following mip with a compute dispatch and a restricted previous-mip SRV/current-mip UAV. Nearest uses standard min/reversed max; farthest uses standard max/reversed min. Only requested reduction products are generated. Coverage and far clear values remain consistent with the source view. NPOT reductions conservatively cover the complete source footprint, including odd edges and single-dimensional chains; tracing uses matching actual mip sizes.

### Contact visibility and pipeline placement

The order is BasePass -> requested HZB -> fullscreen contact mask -> Deferred lighting -> compatibility -> sky -> transparency -> tonemap. CSM remains an independent lighting input. Reuse the existing oversized-triangle fullscreen mechanism for mask production, proving compute-to-graphics consumption.

Contact traces from valid opaque/masked GBuffer points toward the same normalized main-light direction used by direct lighting. Reconstruct positions with the matching inverse view projection and viewport. Use geometric-normal/start bias, finite world-space length and thickness, projected cell traversal with mip descent near possible intersections, and final full-resolution depth verification. Screen/near-plane clipping, invalid pixels, depth discontinuities and bounded traversal are explicit. Deterministic tracing avoids a dependency on temporal denoising. Edge/distance fades limit discontinuities; unresolved/offscreen rays contribute neutral visibility.

Store visibility with 1 lit and 0 occluded. Deferred directional direct lighting uses min(contact, CSM) in both depth conventions. Indirect/environment/emissive and unrelated local lights are unchanged. CSM and contact effect switches are independent, while the selected light's shadow eligibility is respected. Min can fill missing shadows but cannot brighten CSM acne; single-layer screen depth cannot recover offscreen/hidden occluders.

Later surfaces cannot blindly sample another surface's mask. A future Forward extension requires matching receiver depth before mask production. Unreal's documented Forward renderer excludes contact shadows; HDRP prepares depth first and shares contact results with Deferred and Forward opaque lighting. References: [Unreal contact](https://dev.epicgames.com/documentation/en-us/unreal-engine/contact-shadows-in-unreal-engine), [Unreal Forward](https://dev.epicgames.com/documentation/en-us/unreal-engine/forward-shading-renderer-in-unreal-engine), [HDRP modes](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/Forward-And-Deferred-Rendering.html), [HDRP graph](https://github.com/Unity-Technologies/Graphics/blob/master/Packages/com.unity.render-pipelines.high-definition/Runtime/RenderPipeline/HDRenderPipeline.RenderGraph.cs).

### Controls and evidence

DebugUI edits runtime settings through the existing Main-owned settings -> frozen frame -> pipeline Configure path. Include enabled, length, thickness, bias and step budget, plus mask/mip display and active producer/dispatch/memory statistics. Depth convention remains startup-frozen. Forward reports inactive contact while retaining the requested setting.

Validation includes portable shader/reflection/cache tests, deterministic compute results with parameter/shader replacement, resource and graph negatives, more than 16 logical passes, full-precision GPU/CPU HZB comparison, contact fixtures and the current ready Sponza scene. Run Standard/Reversed, CSM/contact combinations, static/moving camera, dynamic switching, resize and failure/lifetime cases. Measure GPU HZB/mask costs using existing fenced timings, reporting actual samples and any limitations. Run appropriate style, naming, boundaries and Debug/Release suites. Record evidence in implementation.md.

## Risks / Trade-offs

- Broad binding/graph changes -> land and test foundation before effect work; preserve existing graphics defaults and call sites.
- Mip aliasing or stale descriptors -> compare complete view identities and validate physical overlap before recording.
- NPOT hierarchy gaps -> CPU reference and odd-size GPU comparisons; conservative footprint coverage.
- Recording batching changes retention/timings -> exercise mixed passes, failure recovery and distinct per-pass queries.
- Screen-space artifacts -> short physical rays, bounded thickness, geometric bias and explicit visibility limits.
- Performance benefit is workload dependent -> report measured HZB plus mask cost, including moving Sponza and disabled overhead.

## Migration Plan

Add compatible fields/enums/interfaces, extend compiler and RHI, then graph/recording and Renderer compute, then HZB/contact/controls. Preserve existing serialized identifiers and enum values where persisted. New settings have defaults; no asset migration is required. A runtime contact toggle returns lighting to its current behavior and removes unused producers. No Git commit or automatic archive is part of this delivery.

## Open Questions

No user decision blocks implementation. Numeric defaults and any bounded format fallback will be selected from deterministic fixture and Sponza evidence and recorded with validation.
