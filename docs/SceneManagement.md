# Scene management and Scene Viewer

## Run

From the repository root:

```powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --config experiments/Scene.json
./out/build/debug/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --scene /Game/Scenes/Showcase.hasset --scene-culling bvh
```

`experiments/Scene.json` opens the Khronos Sponza atrium at a fixed initial browsing view, with panels initially visible (Tab toggles them). The Scene panel selects native sky assets and edits sky intensity, yaw and background visibility; see [SkyLighting.md](SkyLighting.md) for importing custom HDR/EXR environments and the three shipped options. Published sample content comes from HyperionAssets; mount setup and explicit source recovery are described in [ContentFileSystem.md](ContentFileSystem.md). See [Sponza migration](SponzaMigration.md) for source provenance and view calibration. The initial browsing view has a vertical field of view of approximately 41.78 degrees. Projection and CSM use the current viewport lens; no authored camera object is needed. ModelViewer explicitly creates its own camera with a 1-radian vertical field of view.

The separately selectable Showcase example contains 78 model instances plus one shared ground slab (79 instances total). `../HyperionAssets/.cache/Sources/Models/Ground.gltf` reuses the repository-generated Showcase cube geometry with a matte gray material and embeds its buffer, so no external download or texture is required. The slab spans X=[-35,35], Z=[-40,5], with its top at Y=0 and thickness 0.4. Showcase instances are raised to Y=0.192 to place their scaled pedestal bottoms on the slab; Interleaved instances already start at Y=0. All 78 model instances sit on the top surface and fit within its edges. `--scene-culling` accepts `none`, `linear` and `bvh`. Existing ModelViewer configuration, `--model`, capture and executable names remain supported. A configuration cannot select both model_source and scene_source.

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

Add reuses a loaded asset and works after deleting every instance. The Save edited scene button writes the edited snapshot asynchronously to <name>.edited.hasset; --save-scene PATH selects a CLI output. Stable IDs, parent links, local matrices, enabled/visibility flags, material overrides, every camera/light payload, the three scene selections and optional initial view persist. Ordinary navigation never updates this preset or authored camera objects. Fit adjusts the independent browsing lens to the model bounds.

`FSceneInstance::RegisterModelAsset` prepares additional native model references on Main without creating a node or changing the document revision. Identical resolved references share preparation, visible through `GetAssets`; `AddNode` consumes the returned asset ID. This also works in a new empty instance. Snapshots emit only references used by live nodes, independent of the originally loaded manifest. Unused failed or cached registrations do not block unrelated scene models. Load/Close cancel and join all registrations. The Editor uses this path for [Place Object](Editor.md#放置对象).

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

Paths resolve relative to the manifest. Each object has a stable ID and registered components. Camera, model and light capabilities can coexist; built-in types are unique per object. Objects without these capabilities display as Group. Transform is mandatory. Parents may appear after their children. A node accepts either an affine column-major `transform` or TRS (`translation`, normalized `[x,y,z,w]` `rotation`, `scale`), never both. Missing parents, cycles, duplicate IDs, invalid poses/lenses and selections of the wrong kind reject the whole manifest before installation.

Native `hyperion.scene` records use schema v7; reflected nodes use v3 component envelopes. Version 7 adds optional reflected `initialView` metadata. Version-six reads retain every authored camera and do not infer a preset or remove nodes. Native v4/v5 nodes migrate their typed payloads into components. Native v1/v2/v3 and plain source v1 retain their compatibility conversion of old eye/target and defaults into real nodes with deterministic collision-free IDs. Native v4 migrates without adding lights. Source v2 remains readable; source v3 adds point/spot payloads. Empty scenes stay empty. Scene owns the records and migrations; AssetImport owns the private source JSON adapter. The scene-json and native-scene-upgrade importer revisions are 7. AssetTool `--scene` creates one whole-model instance plus default lights, with no camera object; internal model nodes and primitives stay asset-owned. Scene publication records both model and browsing policies to invalidate older caches. Existing expanded source documents retain their authored topology and edits. `inspect` keeps `instances=` model-component-only. Canonical record JSON supports initial views and arbitrary registered component state; see [SceneComponents.md](SceneComponents.md), [NativeAssets.md](NativeAssets.md) and [LocalLights.md](LocalLights.md).

Native node records reject unknown fields that could hide an unsupported payload or misspelled kind. Native camera records also reject unsupported projection fields. Other reflection records retain their usual unknown-field diagnostics.

## CPU ray queries

`FScene::Raycast(FRay, FSceneRayOptions)` queries current Main-owned static geometry. Scene normalizes the input direction; Minimum/Maximum and returned Distance are world distances. Results contain Hit/Miss/Unavailable, a generation-checked scene handle, instance/primitive/triangle indices, position and barycentrics. A Hit with `bIncomplete` is the nearest prepared geometry while other candidates remain unavailable. A query with no hit and unavailable candidates returns Unavailable, not a confirmed Miss.

`PrepareSceneModelGeometry` builds compact immutable per-primitive triangle BVHs; attach the result to `FSceneModelData::QueryGeometry` before publishing the data. `FSceneInstance` and `LoadNativeModel` accept an optional `bInPrepareQueries` flag, default false. Editor enables it so preparation runs on Worker with the existing cancellation/join lifecycle. Consumers providing explicit model data prepare and attach their own query geometry; it is never built implicitly on Main. Instances sharing model data share its triangle acceleration.

The scene object index is allocated lazily and updated from successful transactions independently of Render's acknowledged changes. Bounds transforms refit the tree; topology changes rebuild it. Camera/light/metadata changes do not rebuild model geometry. Main and Render own separate mutable indices using Math's bounds BVH algorithm. `FSceneRayStats` reports builds/refits, candidates, instance tests, triangle nodes and exact triangle tests. Material options specify two-sided geometry or an ordered list of scheduled pass usages with per-usage exclusions for culling. Renderer's `MakeSceneRayOptions` supplies the Forward or Deferred pipeline policy, including legacy Forward exclusions. Reported hit distances use float precision; traversal retains rounded-distance ties without widening the original clipping interval. These are geometric queries, without alpha-discard or shader-deformation evaluation.

Renderer's `MakeViewportRay` uses normalized image coordinates (top-left origin), the actual pixel extent and the shared scene camera projection construction. It supports both depth conventions and returns a world-distance interval clipped to the camera's near/far planes.

## Runtime scene entity

`FSceneInstance` is the Main-owned Renderer entity alongside `FModel`. Construct it with a render session, task system and asset service; call `Load(path)`, `Tick()` on every Main tick (including minimized frames), then `Close()` before closing the session. It owns the logical `FScene`, its `FSceneRenderBridge`, asynchronous manifest/model requests and instance-to-asset bookkeeping. Register native descriptors with `RegisterSceneAssetTypes(Assets.Types())` before loading.

`Add`, `Update`, `Remove`, `Find` and `GetHandles` use generation-checked handles. Add can use ready model data or an asset ID from the loaded manifest. Transform/visibility edits and removal during loading survive completion; explicit data replacement detaches the pending asset association. `Snapshot(destination)`, `GetModels`, `GetAssets`, `GetManifest`, `GetStatus`, `GetError` and `GetDrawResults` expose structured data without GUI strings. Per-handle errors include associated asset loading failures and reject stale generations. Close cancels requests and joins all admitted preparation before detaching; it is idempotent, and a later Load creates a fresh attachment. After Close, repeated Close and destruction do not access the external dependencies.

The transitional model `Find` projects effective visibility. A roundtrip `Update` preserves authored model visibility when that projected bit is unchanged; use `SetModelVisible` to set the local flag explicitly while the node or an ancestor is disabled. Name, material and world-transform edits do not persist inherited hiding.

Both SceneViewer and ModelViewer own this entity. SceneViewer owns an independent `FSceneCameraView`, initialized from InitialView or deterministic bounds framing, with a stable empty-scene fallback. Its navigation never edits scene objects. ModelViewer retains its selected scene-camera workflow. Plugins keep transient gesture state, selection and diagnostics. CPU Scene remains independent of Renderer/RHI.

## Ownership and synchronization

Construct/use `FScene` on Main. It owns composed scene objects with Transform and optional model/camera/light components, generation-checked handles, stable IDs, local transforms, parent links and explicit scene selections. World transforms and effective enabled flags are derived iteratively from the hierarchy. `FSceneModel` is a compatibility transfer value, while node components own model content and optional immutable `FSceneModelData`. Null data represents a placeholder. `PrepareSceneModel` validates assets and computes node occurrences/bounds on a loading Worker; instances share that metadata. Scene depends on Math/Reflection/AssetTypes/Materials, not Renderer/RHI.

`FSceneRenderBridge` attaches one logical Scene to one render client. Both ends reject duplicate simultaneous logical attachment. Destroy the bridge before its scene, session and task system. It consumes coalesced owned changes in revision order, acknowledges successful admission and observes update task errors associated with the submitted revision. Reattachment republishes all nodes under a fresh attachment epoch.

Renderer `FModel` remains a compatible Main rendering attachment. It owns bindings/resource leases; `FSceneModel` is the model transfer value derived from the logical node. Initial registration carries complete state. Updates and removals each use one Render task for all members. Automatic destruction batches removal using bindings, so an adapter surviving session shutdown need not dereference a destroyed scene.

Flush scene changes every Main tick, including minimized ticks. Loading results attach only to still-live generation-checked handles. Stop cancels consumers, joins preparation and removes attachments. Render removal does not wait for GPU completion; existing frame leases, resource coordinator and fences preserve native ownership. No logical pointer is borrowed by Render.

## Camera/light publication and frame inputs

Apply all Main edits and completed loads, call `Tick()`/bridge `Flush()`, then `FreezeSceneFrame(token)` immediately before submitting the frame. The immutable seed includes logical scene identity, attachment epoch, publication serial and logical revision. Render resolves only the exact applied token; a stale token fails before drawing. Camera/light metadata and geometry changes use one Render publication. Camera/light-only edits do not prepare model transfers or invalidate the geometry BVH. `GetModelPreparationCount()` and benchmark `index_rebuilds`/`index_refits` expose these invariants without timers.

`FSceneViewRequest` supplies dimensions, viewport, depth convention, view identity and an optional camera handle. An unavailable explicit camera falls back to the selected default only when `bAllowCameraFallback` is true (the runtime default); Editor preview sets it false. A foreign-scene handle is an error. `CameraOverride` carries an independent view value. With no usable view, pipelines clear the current output and continue GUI/completion. A camera uses local -Z forward and +Y up; pose extraction orthonormalizes inherited scale. `FocusDistance` controls interaction, not projection.

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

`FSceneCameraController` in `Hyperion/Renderer/SceneCameraController.h` provides WASDQE, right-button orbit and wheel dolly without a SceneViewer or GUI dependency. Create one per viewport. On Main, call `Input(ViewCamera, Events, bMouseCaptured, bKeyboardCaptured)` then `Advance(ViewCamera, DeltaSeconds)` before freezing the view request. Scene overloads remain available for callers intentionally controlling an authored camera. Call `Reset()` when deactivating or minimizing a viewport, changing its scene or shutting down. Input also clears movement on focus loss or keyboard capture. A fresh press resumes after interruption.

Movement needs no mouse button. Speed is `max(1, FocusDistance)` world units/second; combined directions are normalized. Frame time is capped at 0.1 seconds to limit jumps after stalls. Release stops immediately without inertia. RMB continues to orbit the focus pivot; wheel changes focus distance.
