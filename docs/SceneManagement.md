# Scene management and Scene Viewer

## Run

From the repository root:

```powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json
./out/build/debug/bin/hyperion_viewer.exe --scene out/content/Scenes/Showcase.hasset --scene-culling bvh
```

`experiments/Scene.json` opens the Khronos Sponza atrium at a fixed reference-matched entrance camera, with panels initially visible (Tab toggles them). The Scene panel selects native sky assets and edits sky intensity, yaw and background visibility; see [SkyLighting.md](SkyLighting.md) for importing custom HDR/EXR environments and the three shipped options. The complete source asset is included and the normal Viewer build imports it to `out/content/Scenes/Sponza.hasset`. See [Sponza migration](SponzaMigration.md) for source provenance and camera calibration. The shipped scene camera has a vertical field of view of approximately 41.78 degrees. Projection, CSM and Fit read the selected camera node; its lens is editable. ModelViewer explicitly creates its own camera with a 1-radian vertical field of view.

The separately selectable Showcase example contains 78 model instances plus one shared ground slab (79 instances total). `assets/Models/Ground.gltf` reuses the repository-generated Showcase cube geometry with a matte gray material and embeds its buffer, so no external download or texture is required. The slab spans X=[-35,35], Z=[-40,5], with its top at Y=0 and thickness 0.4. Showcase instances are raised to Y=0.192 to place their scaled pedestal bottoms on the slab; Interleaved instances already start at Y=0. All 78 model instances sit on the top surface and fit within its edges. `--scene-culling` accepts `none`, `linear` and `bvh`. Existing ModelViewer configuration, `--model`, capture and executable names remain supported. A configuration cannot select both model_source and scene_source.

| Control | Action |
| --- | --- |
| Right-button drag / wheel | Orbit around target / dolly |
| W/S, A/D | Hold to move forward/back along the view direction or strafe left/right |
| Q/E | Hold to move down/up along world Y |
| Arrows / Page Up, Page Down | Continuous aliases for WASD / E,Q |
| Home / C / Tab | Fit all loaded visible models / cycle culling / toggle panels |
| Insert / Delete / Space | Duplicate one selected node / remove its subtree / toggle selected model visibility |
| Scene panel | Hierarchical node selection, typed properties, creation, reparenting, two removal modes and model animation |
| Freeze culling camera | Retain rejection view while display camera moves |
| Show model bounds | Overlay bounds; diagnostic projection does not determine visibility |

Add reuses a loaded asset and works after deleting every instance. The Save edited scene button writes the edited snapshot asynchronously to <name>.edited.hasset; --save-scene PATH selects a CLI output. Stable IDs, parent links, local matrices, enabled/visibility flags, material overrides, every camera/light payload and the three scene selections persist. Near/far limits come from the camera node; very large scenes need a suitable far limit even after Fit.

## Source manifest (offline import)

```json
{
  "type": "hyperion.scene",
  "schema_version": 3,
  "assets": [{"id": "model", "path": "../Models/Showcase.gltf"}],
  "nodes": [
    {"id": "rig", "translation": [0, 0, 0]},
    {"id": "left", "parent": "rig", "translation": [-3, 0, 0],
     "model": {"asset": "model", "visible": true}},
    {"id": "camera", "translation": [0, 0, 12],
     "camera": {"verticalRadians": 0.729224966, "near": 0.01,
                "far": 1000, "focusDistance": 12}},
    {"id": "sun", "directionalLight": {
      "color": [1, 0.95, 0.9], "intensity": 3, "castShadows": true}},
    {"id": "ambient", "environmentLight": {
      "color": [0.22, 0.25, 0.3], "intensity": 1}}
  ],
  "defaultCamera": "camera",
  "mainDirectionalLight": "sun",
  "environmentLight": "ambient"
}
```

Paths resolve relative to the manifest. Each node has a unique stable ID and at most one model, camera, directional-light, environment-light, point-light or spot-light payload; no payload means Group. Parents may appear after their children. A node accepts either an affine column-major `transform` or TRS (`translation`, normalized `[x,y,z,w]` `rotation`, `scale`), never both. Missing parents, cycles, duplicate IDs, invalid poses/lenses and selections of the wrong kind reject the whole manifest before installation.

Native `hyperion.scene` records use schema v5; reflected nodes use v2. Native v1/v2/v3 and plain source v1 migrate explicitly, converting the old eye/target and defaults into real nodes with deterministic collision-free IDs. Native v4 migrates without adding lights. Source v2 remains readable; source v3 adds point/spot payloads. Empty scenes stay empty. Scene owns the records and migrations; AssetImport owns the private source JSON adapter. The scene-json and native-scene-upgrade importer revisions are 4, including sky dependencies. AssetTool `--scene` creates one model node plus explicit default camera/lights, and `inspect` keeps `instances=` model-only. See [NativeAssets.md](NativeAssets.md) and [LocalLights.md](LocalLights.md).

Native node records reject unknown fields that could hide an unsupported payload or misspelled kind. Native camera records also reject unsupported projection fields. Other reflection records retain their usual unknown-field diagnostics.

## Runtime scene entity

`FSceneInstance` is the Main-owned Renderer entity alongside `FModel`. Construct it with a render session, task system and asset service; call `Load(path)`, `Tick()` on every Main tick (including minimized frames), then `Close()` before closing the session. It owns the logical `FScene`, its `FSceneRenderBridge`, asynchronous manifest/model requests and instance-to-asset bookkeeping. Register native descriptors with `RegisterSceneAssetTypes(Assets.Types())` before loading.

`Add`, `Update`, `Remove`, `Find` and `GetHandles` use generation-checked handles. Add can use ready model data or an asset ID from the loaded manifest. Transform/visibility edits and removal during loading survive completion; explicit data replacement detaches the pending asset association. `Snapshot(destination)`, `GetModels`, `GetAssets`, `GetManifest`, `GetStatus`, `GetError` and `GetDrawResults` expose structured data without GUI strings. Per-handle errors include associated asset loading failures and reject stale generations. Close cancels requests and joins all admitted preparation before detaching; it is idempotent, and a later Load creates a fresh attachment. After Close, repeated Close and destruction do not access the external dependencies.

The transitional model `Find` projects effective visibility. A roundtrip `Update` preserves authored model visibility when that projected bit is unchanged; use `SetModelVisible` to set the local flag explicitly while the node or an ancestor is disabled. Name, material and world-transform edits do not persist inherited hiding.

Both SceneViewer and ModelViewer own this entity. Their input helpers read and edit the selected scene camera for orbit/dolly/pan/Fit; external transform or parent edits are therefore immediately respected. Plugins keep transient gesture state, selection and diagnostics. CPU Scene remains independent of Renderer/RHI.

## Ownership and synchronization

Construct/use `FScene` on Main. It owns Group/Model/Camera/DirectionalLight/EnvironmentLight/PointLight/SpotLight nodes, generation-checked handles, stable IDs, local transforms, parent links and explicit scene selections. World transforms and effective enabled flags are derived iteratively from the hierarchy. `FSceneModel` is a compatibility transfer value, while node components own model content and optional immutable `FSceneModelData`. Null data represents a placeholder. `PrepareSceneModel` validates assets and computes node occurrences/bounds on a loading Worker; instances share that metadata. Scene depends on Math/Reflection/AssetTypes/Materials, not Renderer/RHI.

`FSceneRenderBridge` attaches one logical Scene to one render client. Both ends reject duplicate simultaneous logical attachment. Destroy the bridge before its scene, session and task system. It consumes coalesced owned changes in revision order, acknowledges successful admission and observes update task errors associated with the submitted revision. Reattachment republishes all nodes under a fresh attachment epoch.

Renderer `FModel` remains a compatible Main rendering attachment. It owns bindings/resource leases; `FSceneModel` is the model transfer value derived from the logical node. Initial registration carries complete state. Updates and removals each use one Render task for all members. Automatic destruction batches removal using bindings, so an adapter surviving session shutdown need not dereference a destroyed scene.

Flush scene changes every Main tick, including minimized ticks. Loading results attach only to still-live generation-checked handles. Stop cancels consumers, joins preparation and removes attachments. Render removal does not wait for GPU completion; existing frame leases, resource coordinator and fences preserve native ownership. No logical pointer is borrowed by Render.

## Camera/light publication and frame inputs

Apply all Main edits and completed loads, call `Tick()`/bridge `Flush()`, then `FreezeSceneFrame(token)` immediately before submitting the frame. The immutable seed includes logical scene identity, attachment epoch, publication serial and logical revision. Render resolves only the exact applied token; a stale token fails before drawing. Camera/light metadata and geometry changes use one Render publication. Camera/light-only edits do not prepare model transfers or invalidate the geometry BVH. `GetModelPreparationCount()` and benchmark `index_rebuilds`/`index_refits` expose these invariants without timers.

`FSceneViewRequest` supplies dimensions, viewport, depth convention, view identity and an optional camera handle. An unavailable explicit camera falls back to the selected default; a foreign-scene handle is an error. With no enabled selected camera, pipelines clear the current output and continue GUI/completion. A camera uses local -Z forward and +Y up; pose extraction orthonormalizes inherited scale. `FocusDistance` controls interaction, not projection.

Global lighting uses the selected `MainDirectionalLight` and `EnvironmentLight`. Unselected global lights remain editable candidates. Disabled/deleted/unselected global lights contribute zero; no defaults reappear. Every enabled point/spot light independently participates in deferred local lighting after visibility rejection; these lights do not require a main-light selection. Surface-to-light is the negative world forward of the selected directional light. Color times intensity supplies radiance. CSM additionally requires nonzero radiance, node `castShadows` and the pipeline shadow switch. Shadow resolution, bias, distance and preview remain pipeline quality settings. `--shadow-light` applies one scene edit after loading; benchmark light motion edits the same node.

Bound sessions protect `Engine.Scene.MainDirectionalLightDirection`, `Engine.Scene.MainDirectionalLightColor`, `Engine.Scene.AmbientColor`, `Engine.View.ViewProjection` and `Engine.View.CameraPosition` from custom provider/Global/Frame/Scene/View/pass injection. Custom unrelated material inputs remain supported. Unbound explicit primitive/view fixtures retain `FreezeFrame()`.

The scene tree offers Group/Camera/DirectionalLight/EnvironmentLight/PointLight/SpotLight creation, single-node duplication, typed properties and explicit default/main selection. Reparent requires KeepLocal or KeepWorld; removing one node while keeping children preserves their world transforms. Invalid cycles/inverses are visible errors. Empty model collections can still edit and save cameras/lights and add a loaded model again.

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

None disables spatial rejection but retains hidden/resource validation. Linear/BVH share conservative tests. Frustum/PVS/software/GPU occlusion remain distinct from BVH/BSP spatial organization. Future GPU visibility requires graph resources/asynchronous execution and cannot be forced into a CPU bool-only interface. Logical parenting is independent of the geometry BVH. Deformation, LOD and occlusion remain separate capabilities.

## Verification

- `scene_runtime_instance`: plugin-free loading, shared data, loading-time edits/removal/replacement, failures, reload and pending close.
- `scene_management`: independent clip-corner oracle, Main ownership, generations, snapshots and manifest validation.
- `scene_spatial_visibility`: 4097 groups, randomized updates/removal, BVH/linear equivalence and multi-item early collection.
- `scene_rendering`: real GPU bridge sharing, initial transform, reattachment and no-frame removal.
- `render_resources`: pending bounds becoming indexed and clip-space fallback.
- `scene_viewer_controls`: real readbacks for hide/move/duplicate/remove, frozen view, fit and empty-scene recovery.
- `scene_viewer_acceptance`: identical PNGs across modes for 514 instances, GUI, malformed manifests and isolated asset failure.

Build-local screenshots/logs live in `out/build/<preset>/scene-acceptance`. Measured Debug fixture: 514 groups / 2053 registered primitives. None collected/submitted 2053 items; Linear tested 514 groups and collected 5 primitives; BVH visited 19 nodes, tested 2 leaves and collected the same 5. Images were byte-identical. These are fixture counts, not a general frame-rate guarantee. Final evidence is recorded in the archived [verification](../openspec/changes/archive/2026-09-07-add-scene-management/verification.md) and [independent review](../openspec/changes/archive/2026-09-07-add-scene-management/independent-review.md).

## Scene frame ownership

A resolved scene frame is immutable and authorized only as its original shared instance. Copying its public material inputs does not transfer that authorization; validation, semantic resolution and view-family construction reject such copies for bound scenes. Unbound explicit material frames remain supported.

`FRenderSession::BuildViews` is low-level plumbing for trusted derived view families, including CSM projections. Scene-bound application code enters through `FSceneViewRequest` and a scene frame seed on the render pipelines. The high-level explicit-view overload rejects a bound scene; low-level family construction intentionally accepts internally derived views with the resolved frame.

### Reusable camera navigation

`FSceneCameraController` in `Hyperion/Renderer/SceneCameraController.h` provides WASDQE, right-button orbit and wheel dolly without a SceneViewer or GUI dependency. Create one per viewport. On Main, call `Input(Scene, Events, bMouseCaptured, bKeyboardCaptured)` then `Advance(Scene, DeltaSeconds)` before publishing the scene. Call `Reset()` when deactivating or minimizing a viewport, changing its scene or shutting down. Input also clears movement on focus loss or keyboard capture. A fresh press resumes after interruption.

Movement needs no mouse button. Speed is `max(1, FocusDistance)` world units/second; combined directions are normalized. Frame time is capped at 0.1 seconds to limit jumps after stalls. Release stops immediately without inertia. RMB continues to orbit the focus pivot; wheel changes focus distance.
