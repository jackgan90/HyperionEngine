# Scene management and Scene Viewer

## Run

From the repository root:

```powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json
./out/build/debug/bin/hyperion_viewer.exe --scene assets/Scenes/Showcase.json --scene-culling bvh
```

The example contains 78 model instances plus one shared ground slab (79 instances total). `assets/Models/Ground.gltf` reuses the repository-generated Showcase cube geometry with a matte gray material and embeds its buffer, so no external download or texture is required. The slab spans X=[-35,35], Z=[-40,5], with its top at Y=0 and thickness 0.4. Showcase instances are raised to Y=0.192 to place their scaled pedestal bottoms on the slab; Interleaved instances already start at Y=0. All 78 model instances sit on the top surface and fit within its edges. `--scene-culling` accepts `none`, `linear` and `bvh`. Existing ModelViewer configuration, `--model`, capture and executable names remain supported. A configuration cannot select both model_source and scene_source.

| Control | Action |
| --- | --- |
| Mouse drag / wheel | Orbit around target / dolly |
| Arrows / Page Up, Page Down | Translate camera and target / elevate |
| Home / C / Tab | Fit all loaded visible models / cycle culling / toggle panels |
| Insert / Delete / Space | Duplicate / remove / show or hide selected model |
| Scene panel | Select next model, move along X, animate movement, add a loaded model |
| Freeze culling camera | Retain rejection view while display camera moves |
| Show model bounds | Overlay bounds; diagnostic projection does not determine visibility |

Add reuses a loaded asset and works after deleting every instance. Runtime edits do not overwrite the manifest. Near/far limits come from the manifest; very large scenes need a suitable far limit even after Fit.

## Manifest

```json
{
  "type": "hyperion.scene",
  "schema_version": 1,
  "assets": [{"id": "model", "path": "../Models/Showcase.gltf"}],
  "instances": [
    {"id": "left", "asset": "model", "translation": [-3, 0, 0]},
    {"id": "right", "asset": "model", "translation": [3, 0, 0],
     "rotation": [0, 0, 0, 1], "scale": [1, 1, 1], "visible": true}
  ],
  "camera": {"eye": [0, 3, 12], "target": [0, 0, 0], "near": 0.01, "far": 1000}
}
```

Paths resolve relative to the manifest. Asset and instance IDs are nonempty and unique within their respective arrays. References must resolve. Transforms use finite translation/scale and normalized quaternion `[x,y,z,w]`. Camera direction must be valid and not parallel to world up. A malformed manifest reports a terminal error. Individual model failures leave other instances active and appear in the panel. Manifest IDs are separate from runtime generation-checked handles.

Scene owns the validated record schema and private JSON adapter. `RegisterSceneManifestLoader` registers the codec with AssetService before requests start; model files use the existing glTF importer. Reflected `.hasset` manifests also work through AssetService. The adapter privately uses the existing JSON dependency.

## Runtime scene entity

`FSceneInstance` is the Main-owned Renderer entity alongside `FModel`. Construct it with a render session, task system and asset service; call `Load(path)`, `Tick()` on every Main tick (including minimized frames), then `Close()` before closing the session. It owns the logical `FScene`, its `FSceneRenderBridge`, asynchronous manifest/model requests and instance-to-asset bookkeeping. Register `RegisterSceneManifestLoader` from the Renderer runtime before loading.

`Add`, `Update`, `Remove`, `Find` and `GetHandles` use generation-checked handles. Add can use ready model data or an asset ID from the loaded manifest. Transform/visibility edits and removal during loading survive completion; explicit data replacement detaches the pending asset association. `GetModels`, `GetAssets`, `GetManifest`, `GetStatus`, `GetError` and `GetDrawResults` expose structured data without GUI strings. Per-handle errors include associated asset loading failures and reject stale generations. Close cancels requests and joins all admitted preparation before detaching; it is idempotent, and a later Load creates a fresh attachment. After Close, repeated Close and destruction do not access the external dependencies.

SceneViewer now owns this entity and keeps camera, input, selection, visualization and the original fixed-step demo animation. ModelViewer continues using `FModel`. CPU Scene remains independent of Renderer/RHI; no gameplay object hierarchy is introduced.

## Ownership and synchronization

Construct/use `FScene` on Main. It owns flat `FSceneModel` values with name, transform, visibility, material overrides and optional immutable `FSceneModelData`. Null data represents a placeholder. `PrepareSceneModel` validates assets and computes node occurrences/bounds on a loading Worker; instances share that metadata. Scene depends on Math/Reflection, not Renderer/RHI.

`FSceneRenderBridge` attaches one logical Scene to one render client. Both ends reject duplicate simultaneous logical attachment. Destroy the bridge before its scene, session and task system. It consumes coalesced owned changes in revision order, acknowledges successful admission and observes update task errors associated with the submitted revision. Reattachment republishes existing models.

Renderer `FModel` remains a compatible Main rendering attachment. It owns bindings/resource leases; `FSceneModel` is the independent logical record. Initial registration carries complete state. Updates and removals each use one Render task for all members. Automatic destruction batches removal using bindings, so an adapter surviving session shutdown need not dereference a destroyed scene.

Flush scene changes every Main tick, including minimized ticks. Loading results attach only to still-live generation-checked handles. Stop cancels consumers, joins preparation and removes attachments. Render removal does not wait for GPU completion; existing frame leases, resource coordinator and fences preserve native ownership. No logical pointer is borrowed by Render.

## Visibility flow

1. Render applies mailbox updates and refreshes changed group/primitive bounds.
2. Previously unavailable bounds are reconsidered independently of visibility.
3. `ISceneSpatialIndex` queries group IDs using an independent `ISceneVisibility` volume test.
4. Candidate members receive primitive bounds tests before `Collect`.
5. Candidates return to registry slot order, preserving transparent ties. Primitives still emit zero or multiple items.
6. Existing readiness/section validation, finer item frustum filtering, global transparency and pass construction continue.

Each `CreateBatch` creates one group; `Create` creates a one-member group. BVH nodes do not represent logical parents. Index implementation details remain private; FRenderScene initially installs the BVH implementation. Linear mode supplies a reference traversal with the same group and primitive tests.

`IRenderPrimitive::GetWorldBounds` is Render-only/read-only. Its conservative bounds must cover every possible output and remain valid until a state update; custom primitives default to unknown. Static primitives use CPU bounds or ready geometry metadata and check the description for clip-space semantics. While the description is unavailable, their groups remain unbounded and are reconsidered without depending on a camera seeing them. Unknown members make their group unbounded. Clip-space content bypasses world-space rejection and retains item-level clipping.

Math uses column vectors, affine world transforms and RH zero-to-one clip conventions, with conservative epsilon. Unknown/nonfinite/projective bounds fail open. Frozen culling changes only the rejection view: drawing and transparent sorting use the display view. Recollect for each camera; a culled snapshot cannot reveal objects removed for an earlier camera. For item-only experiments, collect with None first.

## BVH maintenance and statistics

The binary BVH uses deterministic median splits, one leaf per bounded group and an unbounded set. Topology changes batch into one rebuild before querying. Changed bounds refit their leaf and ancestors. A normalized area-cost increase above twice the last build cost triggers rebuilding. Unchanged frames/camera motion do not rebuild. Bulk per-frame insertion/removal still costs a rebuild; overlap and all-visible scenes need not benefit from traversal.

Render publishes last-frame statistics. Groups, registered/candidate primitives, Collect calls, emitted items, visible items and draw packets are separate counters. BVH visits include internal and leaf tests; linear group tests count bounded groups. Index update time includes group bounds/tree maintenance; query time covers group traversal rather than the full frame. Rebuild/refit counters are per-frame. Scene draws exclude GUI/native non-scene passes.

None disables spatial rejection but retains hidden/resource validation. Linear/BVH share conservative tests. Frustum/PVS/software/GPU occlusion remain distinct from BVH/BSP spatial organization. Future GPU visibility requires graph resources/asynchronous execution and cannot be forced into a CPU bool-only interface. Parenting, deformation, LOD, GPU instancing and occlusion implementations remain future work.

## Verification

- `scene_runtime_instance`: plugin-free loading, shared data, loading-time edits/removal/replacement, failures, reload and pending close.
- `scene_management`: independent clip-corner oracle, Main ownership, generations, snapshots and manifest validation.
- `scene_spatial_visibility`: 4097 groups, randomized updates/removal, BVH/linear equivalence and multi-item early collection.
- `scene_rendering`: real GPU bridge sharing, initial transform, reattachment and no-frame removal.
- `render_resources`: pending bounds becoming indexed and clip-space fallback.
- `scene_viewer_controls`: real readbacks for hide/move/duplicate/remove, frozen view, fit and empty-scene recovery.
- `scene_viewer_acceptance`: identical PNGs across modes for 514 instances, GUI, malformed manifests and isolated asset failure.

Build-local screenshots/logs live in `out/build/<preset>/scene-acceptance`. Measured Debug fixture: 514 groups / 2053 registered primitives. None collected/submitted 2053 items; Linear tested 514 groups and collected 5 primitives; BVH visited 19 nodes, tested 2 leaves and collected the same 5. Images were byte-identical. These are fixture counts, not a general frame-rate guarantee. Final evidence is recorded in the archived [verification](../openspec/changes/archive/2026-09-07-add-scene-management/verification.md) and [independent review](../openspec/changes/archive/2026-09-07-add-scene-management/independent-review.md).
