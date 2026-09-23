# Capability implementation inventory

Exact schemas and availability come from the target catalog. The maintained task/operation/service map is [AutomationCapabilities.md](../../../docs/AutomationCapabilities.md); this change inventory records implementation and validation evidence.

| Existing human task | Shared owner / adapter | Evidence |
|---|---|---|
| Query scene, transform, save, undo/redo | SceneEditing document; existing IDs retained | automation_scene, automation_attachment; saved reopen and shared history |
| Ordered selection, metadata, create, delete subtrees, reparent | SceneEditing authoring transactions | CPU atomicity/hierarchy/history tests; live group/camera creation, selection and metadata |
| Inspect/add/remove/edit components and scene settings | SceneEditing + registered component records | CPU component/value/atomic rejection tests; live camera edit and lazy schema discovery |
| Primitive/light placement and scene references | IScenePlacement; existing ObjectPlacement and scene resource preparation | live Editor Cube / Viewer loaded-model placement; editor_placement; component reference records preserve immutable model bindings |
| Open/clear scene, status, dirty decisions | ISceneDocumentHost; shared Editor load/reset path | live saved reopen, missing-file failure and recovery; editor_acceptance |
| Choose/clear asset root and persist preferences | ContentRootService and existing host participants; one automation-assets registration owner | live dirty rejection, clear/set and handle invalidation; editor_content_transition/startup; no-GUI regression |
| Find native assets/scenes | Content/Assets index, bounded content.assets.list | schema discovery and asset import/reopen; results are references, not bulk data |
| Open/activate/close actual asset tabs, save/history | IAssetWorkspace backed by Editor; CPU documents without workspace | two simultaneous MCP/JSONL clients share identity, dirty state and undo; editor_asset_workspace/documents |
| Save all / close all | list documents, then existing per-document save/close | Same per-document save/conflict semantics; no new batch atomicity promise |
| Model node transform/name, primitive name/material slots | AssetEditing topology validation + prepared reference graphs | live imported model edit/save, failed reference leaves generation unchanged; GUI asset regressions |
| Material numeric/vector/array/matrix/texture values | Shared material editability, clamps and reference validation; recursive reflected schemas | recursive wire/schema CPU tests; live material roughness clamp; editor_asset_editors |
| Texture name/encoding/pixel; read-only sky products | Existing encoding service; bounded texture.sample; reflected sky fields | texture CPU/transport tests, live pixel query; existing GUI property/preview regressions |
| Asset preview controls | IAssetPreviewWorkspace using Editor preview state | live texture channel/zoom with unchanged document generation; preview screenshot acceptance |
| Browsing camera/frame/speed/exposure; scene-camera authoring | ISceneViewport + SceneCameraController; Editor document methods | live temporary view changes and camera authoring; editor_acceptance view/input/gizmo tests |
| Outlines/light markers; Viewer culling/freeze/batching/bounds/animation | Shared host options | live Viewer control changes; scene_viewer_controls, editor_outlines/multiselect |
| Render readiness/statistics/component diagnostics and PNG | Renderer snapshots and host readback; bounded artifact metadata | live primitive diagnostics, main/asset/Viewer screenshots; image completion and file presence |
| RenderDoc preference/capture/replay | IRenderCaptureControl using existing capture path | unavailable invocation and editor_capture_ui; enabled RenderDoc capture requires a configured provider |
| GUI scale / profiling / Viewer configuration / shadows | Shared GUI service, IApplicationSettings and IShadowControls | live scale/config save/stale revision/shadow control; gui_scale_acceptance, profiling contracts, configuration_plugins |
| Import/publication | FAssetImportService with registered production importers and publication leases | live glTF + image/material dependencies, native reopen; non-cancellable job status; async_gltf_import publication regressions |

## Contract details and scope

- Attached scene and asset documents are application-owned, shared by GUI and all connections. Jobs remain connection-owned. Generations/revisions and dirty/busy/readonly checks remain explicit.
- Editor deletion clears selection; Viewer preserves selection of the first remaining model. Editor undo restores the deleted selection with remapped handles. Viewer controls regression checks the shared selection state.
- GUI and agent edits share validation and commit paths. Domain services prepare reference changes before committing; stale or failed work cannot overwrite newer state.
- Asset sequence queries use bounded pages; edits preserve existing topology. Recursive material schemas use local $ref edges; wire budgets are unchanged.
- Temporary views/previews do not dirty documents. Image results name target-local files after write completion. Scene preparation errors are observable and repairable.
- Unsupported host/build/startup controls are unavailable. Optional provider disablement leaves other operation families usable.
- Save/import/publication/capture work is owned and drained; accepted operations advertise their actual non-cancellable state.
- No transport/catalog/session/platform/plugin framework replacement, event streaming, remote file transfer or automatic RPC of all Public C++ methods.
- Docking, native window geometry and raw mouse/key gestures are presentation. Existing AssetTool offline maintenance commands (library migration/validation, Engine generation, measurement and envelope export) remain available as their production CLI; this change does not add MCP wrappers for them.
- No new source formats: PNG/JPEG are imported as model dependencies; supported standalone environment sources use the existing sky importer.

## Audit correction coverage

- Shared default-component admission rejects unprepared model bindings; `automation_scene` tests no mutation/history change.
- Per-type `set_batch` supports different instance IDs and values with one commit/Undo; CPU regression covers mismatched arrays and invalid later targets.
- Workspace entries expose loading/failed metadata and generation zero; real MCP regression closes/reopens a repaired failed tab and captures failed asset/scene windows.
- ModelViewer publishes ISceneViewport; Viewer main light panel and `light.main.get/set` share one method. Real MCP verifies camera/light changes, stale rejection and terminal errors.
- Application normal-close services accept explicit dirty decisions, preserve save failures and drain queued replies before exit. Real MCP/JSONL verifies dirty rejection, failed save, cancel, save/reopen and discard; connection CPU tests cover fragmented shutdown responses.
- Remaining explicit deferred adapter: GUI CPU/FPS/thread/pipeline telemetry is not yet fully represented in structured diagnostics. Optional RenderDoc/Tracy live capture validation requires their configured providers.

## Validation record

Completed validation:

- Full Debug build; Release Editor, Viewer, CLI and affected domain/control/workspace test targets.
- Debug automation contracts, connections, transport, local lifecycle, scene/assets, attachment and capability parity. Release contracts, scene/assets, Viewer controls, workspace and capability parity.
- Editor document/view/input/gizmo/picking acceptance, multi-selection, outlines, placement, asset documents/workspace/editors, content transitions/startup, capture UI, GUI scale and plugin disablement.
- Scene management/instances/failure admission, asynchronous import, material assets, configuration, profiling and sky controls.
- Source formatting and filename checks, semantic naming on all changed/new translation units (then focused rechecks after fixes), module boundaries, git diff whitespace and OpenSpec strict validation.
- Initial no-GUI duplicate content registration and outdated Viewer service/selection fixture failures were resolved and their tests rerun successfully. Final capability acceptance additionally checks camera authoring, asset-window output, import non-cancellable status, content search and stale root generation.

Detailed logs remain local build artifacts under out/AutomationParity*.log. Earlier failure logs are retained as diagnostic history; the Followup/Workspace/Release test logs record successful verification after correction. Maintained documentation does not depend on historical test counts.
