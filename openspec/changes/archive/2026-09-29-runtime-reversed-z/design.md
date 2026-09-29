## Context

Editor commits GUI and automation settings through IRenderSettings on Main but its depth consumers read Options.Rendering instead of the live Rendering value. Asset previews hard-code Reversed. FSceneRenderPipeline already regenerates immutable targets when the view convention changes, CSM regenerates matching depth resources, and material/view/batch/HZB caches include the convention or source identity. Editor joins Render/RHI CPU work each frame; GPU references remain fence-retained.

## Goals / Non-Goals

**Goals:** Next-rendered-frame switching in Editor scene and 3D asset viewports; consistent picking/placement/debug/frozen culling; shared GUI/automation validation and persistence; repeated-switch pixel, lifecycle and resource evidence.

**Non-Goals:** Plugin hot reload, native swapchain FrameDepth reconfiguration, changing custom raw depth materials, automatic saving, scene/asset history edits, guaranteed zero-cost first-switch PSO creation, archive or commit.

## Decisions

1. **One live setting.** Initialize Rendering from startup options and use GetDepthConvention(Rendering.bReversedZ) for active consumers. activeReversedZ denotes the committed convention used by subsequent frames, not a presentation acknowledgement. Avoid a second pending state or asynchronous job because the existing settings service commits synchronously on Main and the renderer can consume it at its existing boundary.
2. **Owned frame arguments.** Freeze rendering configuration before dispatch. Pass the convention by value through the asset window frame into workspace rendering, including windows opened after a switch. Hidden/minimized viewports apply the latest committed value when they next render. Two-dimensional texture previews are unaffected.
3. **Frozen culling remains frozen.** When the setting changes, remap an existing frozen projection by ClipDepthTransform(Reversed), which maps z to w-z in either direction. Keep its physical frustum, camera and frozen flag; do not replace it with the current camera. Validate candidates before updating state.
4. **Reuse resource generations.** Keep scene and shadow targets, HZB products and caches convention-aware. Do not flush all caches or wait for GPU idle just to switch. Both Editor scene pipeline modes, including legacy passes, use offscreen depth; GUI uses only backbuffer color. Thus the unused swapchain depth's optimized-clear value need not change. Direct FrameDepth paths retain their creation-time contract.
5. **Explicit automation semantics.** Keep operation IDs, wire fields and optimistic revision checks. Increment get/set operation metadata to version 2 for the intentional change from startup-only to live depth. Save remains version 1 and explicitly persists current settings. The persistent hyperion.render.settings record stays version 1 because field representation is unchanged. The user-requested semantic replacement is documented; a separate restart-only adapter would expose conflicting settings semantics and is not retained. Failures in candidate validation do not commit settings, frozen state or revision. Later GPU/device failure follows existing rendering failure handling, not a promise of transactional GPU rollback.
6. **Testing actual Editor topology.** Extend GPU tests with offscreen output and unchanged session/swapchain while alternating conventions; verify pixel equivalence, depth-sensitive features and bounded retained resources. Extend Editor automation tests and GUI exercises to prove setting delivery, persistence, interaction/frozen state and preview propagation. Existing startup and direct FrameDepth tests remain useful but do not substitute for this coverage.

## Risks / Trade-offs

- First use of a convention can allocate targets and create PSOs → measure switching through existing statistics; retain lazy resource preparation and check stable-frame reuse.
- CPU joins do not imply GPU completion → retain existing immutable owners/fence retirement and test rapid alternating frames without per-switch WaitIdle.
- Main interactions and rendering can diverge if a consumer retains startup options → audit all Editor reads, capture frame values and test GUI/automation plus frozen culling.
- Setting commit does not mean pixels have been presented → document completion and await a later frame/screenshot for image assertions.
- Asset tabs have independent sessions → pass the convention on every rendered frame rather than updating only the active tab at mutation time.

## Migration Plan

Existing settings files load unchanged. Consumers can discover operation version 2 and the new completion/effects text. Update UI and documentation together with tests. The active delta spec is authoritative for this change until a separately authorized archive; do not synchronize the main spec during implementation. Rollback is restoring the implementation and descriptions; saved booleans remain compatible.

## Open Questions

None blocking. Any unexpected same-swapchain lifetime failure will be reproduced and fixed within the renderer resource contract before declaring completion.
