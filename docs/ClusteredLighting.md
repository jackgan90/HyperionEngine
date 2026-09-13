# Clustered local lighting

Cluster lighting defaults on in the common scene HDR pipeline. CPU Render builds conservative lists; pixel shaders compute the cell index and traverse only that cell's list. No compute/UAV or depth prepass is required. Scene point/spot data, attenuation, light units and serialization are unchanged.

## Routing and control

- Deferred with effective main directional radiance uses `Deferred/LightingClustered`: directional, clusters, environment and emissive in one fullscreen pass.
- Without directional radiance, `Deferred/ClusterLighting` performs cluster/environment/emissive lighting without directional shadow samples. Environment/emissive are still evaluated with zero local lights.
- HDR Forward opaque/masked models and lit transparency in both pipelines query clusters in their model pixel shader. Unlit, shadow depth and legacy display shaders do not evaluate local lights.
- `clustered_lighting` is a default-true application setting, editable in the render pipeline GUI. CLI `--clustered-lighting` and `--no-clustered-lighting` override it. Off restores Deferred volumes and zero Forward/transparent local contribution; valid disabled buffer defaults remain bound to HDR shader interfaces.
- Shadow visualization suppresses local contributions as before. Local shadows remain out of scope.

## View and buffer contract

Grid cells cover 64x64 viewport pixels and 24 logarithmic camera-depth slices over the complete perspective near/far range. Nonempty builds require a valid `FRenderView.Camera` matching the supplied projection. Sub-viewports use their own origin and extent; the last XY tiles may be partial. Standard and reversed Z use the same positive linear depth `dot(World-Eye, Forward)`.

The pixel computes `X/Y = floor((Pixel-ViewportOrigin)/64)`, `Z = floor(log2(Depth)*Scale+Bias)`, then `X + Nx*(Y + Ny*Z)`. CPU computes `Scale = 24/(log2(Far)-log2(Near))` and `Bias = -log2(Near)*Scale`. Valid domain edges are clamped for numerical stability. This is direct buffer addressing, not 3D texture sampling.

| Binding | Stride | Contents |
|---|---|---|
| `ClusterLights` | 64 bytes | Position/inverse range, radiance/type, direction/inner cosine, outer cosine/padding |
| `ClusterHeaders` | 8 bytes | uint offset/count |
| `ClusterIndices` | 4 bytes | uint index into light attributes |

`ClusterViewV1` contains five float4 values: viewport origin/tile reciprocals, grid counts/enabled, camera, forward, logarithmic depth scale/bias/near/far. Registers occupy space 3. Model materials resolve `Engine.View.Cluster*` semantics; fullscreen materials bind the same data explicitly. Custom HDR materials can declare these semantics and include `Lighting/ClusteredLighting.hlsli`.

`LocalLighting.hlsli` owns the common single-light evaluation. An out-of-range or out-of-cone light returns zero, never clips the fragment from within the loop. The old volume shader still discards invalid GBuffer pixels and uses the same single-light function. Per-light numerical bounds remain unchanged; clustered float accumulation has one final HDR target conversion, whereas volumes round after each additive draw.

## Construction and lifetime

`FLocalLightIndex` supplies deterministic candidates using existing None/Linear/BVH visibility. The cluster builder projects the conservative world AABB of each candidate into an XY tile range and derives its depth-slice range. Near/eye crossings conservatively cover full XY. Depth/screen margins protect boundaries. A point uses range-sphere bounds; a spot uses the existing sphere/cone-intersection bounds. The Cartesian cell range is conservative and can contain false positives, which exact pixel attenuation rejects. This initial implementation does not add tighter per-cell sphere/cone tests or normal rejection.

Count/prefix/fill construction produces compact sorted lists without duplicate indices or per-cell truncation. Budgets are 65,536 candidate lights, 1,048,576 cells and 16,777,216 references; exceeding one reports an error rather than dropping lights or silently disabling Forward lighting. Empty storage has valid sentinel records and zero counts.

Assignment keys compare projection, pose, depth/viewport extent and ordered influence bounds. Radiance-only changes update attributes without rebuilding lists. When camera movement recomputes an identical list, exact byte comparison reuses each unchanged header/index source, avoiding uploads and resource-table replacement. Changing viewport origin updates lookup constants; frozen culling affects broadphase but never substitutes the frozen camera for actual cell lookup. Cache reuse is based on full values, not hash equality. The containing frame retains the exact Scene publication; all buffer sources are immutable and queued frames preserve previous generations.

Existing material/RHI structured buffer bindings upload CPU sources. Fullscreen dynamic resource sets use a tracked owner that changes only when buffer bindings change, separate from persistent target/PSO ownership; numeric snapshots use a tracked pass owner. Camera-only numeric changes therefore keep unchanged resource sets alive. Model View scopes and resource-value epochs perform the corresponding retirement. Normal GPU fences protect submitted buffers. No new GPU wait or global cache clearing is used.

Static material preparation uploads sources without creating descriptor sets. Actual draws create/cache sets with their resolved resources. This prevents unused pass defaults from permanently reserving sampler tables alongside real shadow/cluster bindings. `--verify-model` also checks scene draw receipts so descriptor/material failures cannot pass image acceptance as a bright clear background.

The initial D3D12 buffer path uses upload heap memory and allocates new native buffers for changed immutable sources. Stationary frames reuse sources/bindings. Dynamic descriptor allocation counters are cumulative and can rise while live resources remain bounded; GPU memory and live cache entries, rather than that cumulative counter alone, verify retirement. CPU assignment and upload bandwidth are potential limits for dense moving-light workloads; a future GPU builder can retain the consumer contract.

## Assets and diagnostics

Built-in persisted PBR descriptions declare cluster View semantics. The native glTF importer is revision 4, including cluster and sky material semantics; normal builds regenerate stale native sample content. Clustering itself does not require a Scene/native scene schema bump. The renderer supplies valid disabled defaults to standalone built-in material consumers.

Viewer reports algorithm, visible point/spot counts, volume draws, occupied/total cells and reference counts. CSV appends `clustered_lighting`, `cluster_cells`, `cluster_occupied`, `cluster_references`, `cluster_max_lights`, `cluster_bytes`, `cluster_build_ms`, `cluster_rebuilt`. `local_gpu_ms` continues to measure only legacy volume draws; `lighting_gpu_ms` includes the fused or dedicated Deferred lighting pass and is not an isolated local-light timing.

The unchanged Sponza lights and camera are compared against the previous image and the opt-out volume route. See [acceptance](../openspec/changes/archive/2026-09-13-add-clustered-local-lighting/verification.md) for actual image differences, tests and measurements.
