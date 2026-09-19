## Context

The existing registry owns Start/Stop but application classes own graphics, assets, GUI and concrete feature coordination. Main/Render/RHI frames already retain immutable data and drain before destruction. This change preserves those contracts and uses static linking and startup selection only.

## Goals / Non-Goals

**Goals:** A reusable Main loop; dependency-ordered service plugins; independently unavailable optional features; typed service and event contracts; cohesive Viewer/Editor plugins; render-stage extensions; configuration and build selection with useful diagnostics.

**Non-Goals:** DLL ABI, hot reload, runtime reconfiguration of the active plugin set, process fault isolation, a new GPU scheduler, rewriting CPU data libraries, or splitting every editor panel and render pass into a separate binary target.

## Decisions

1. Runtime/Plugins remains independent of graphics. Descriptors retain stable IDs and declare dependencies, optional ordering, services and conflicts. Planning precedes constructors. Explicit disablement wins over dependency expansion; invalid graphs and ambiguous providers are configuration errors. Tolerant activation skips missing/failed optional branches and reports diagnostics; strict activation remains available for compatibility and required command execution.
2. A per-host typed service registry owns registrations, not the service objects. Providers publish only declared types; consumers request declared required/optional services. Scoped registrations disappear before provider destruction. Separate hosts provide separate service scopes. Synchronous typed notifications run on Main; cross-domain work uses TaskSystem with owned values. Plugins can register cleanup and tracked work without introducing a global service locator.
3. A small Runtime/Application host owns Tasks, time, Main pumping and plugin Update. Start/Update/Quiesce/Stop run on the owner thread. An empty host needs no window, device, assets or GUI. Application profiles select plugins and required capabilities. Plugin exceptions stop the run safely; expected feature unavailability is represented by startup diagnostics or feature status.
4. Reusable service plugins own assets/IO, windows, graphics/session and GUI lifetimes. Native backend factories are supplied by application catalogs, preserving backend isolation. RenderDoc uses optional Before/After ordering in this same graph, initializes before graphics, and stops after graphics. Consumers drain frames before reverse dependency shutdown; services remain available during consumer cleanup.
5. Viewer and Editor behavior moves into cohesive application feature plugins. Their executables keep CLI names and select the catalog. Scene/input/status/GUI interactions use domain interfaces rather than concrete scene-plugin casts. Scene producers own time advancement. Editor document/history/panels initially remain cohesive inside its plugin, exposing future panel extension without scattering document invariants.
6. Existing serialized feature fields remain readable. A disabled-plugin list records explicit negative selection. Persisted model_source/scene_source never implies activation; explicit CLI --model/--scene still selects its feature unless disabled. Optional feature CMake switches gate registration and linking. Generic host and service tests exercise zero-feature operation.
7. Renderer owns named feature stages and contexts containing exact view/frame/graph resource identities. Registered features contribute passes through RenderGraph. Contact shadows becomes the first independently selectable built-in feature; existing algorithms and consumer-driven HZB sharing remain in Renderer. The API also exposes scene/UI overlay extension without replacing graph hazard tracking.

## Risks / Trade-offs

The graphics service owns the final GPU validation check: after consumers and RenderSession close, it waits for the GPU and releases the swapchain on RHI 0, reads validation while the device is still alive, and releases the device before reporting any validation failure on Main through FApplicationControl. Both owners move into the RHI task before waiting, so a wait or statistics exception still destroys the swapchain before the device on RHI 0. Earlier Viewer/Editor statistics remain intermediate observations. Native late-error injection belongs only to the existing D3D12 fault-test target; an existing public-contract fake backend verifies cleanup thread and order when the final wait throws.

- [Teardown races] → Quiesce producers, drain frame/task scopes, reverse-stop consumers and only then release provider resources; retain existing GPU fence retirement.
- [Partial startup publication] → Roll back registrations and scoped cleanup for failed instances before starting unrelated branches.
- [Silent configuration errors] → Log complete diagnostics and distinguish unavailable optional branches from explicitly required application capabilities.
- [Configuration migration] → Preserve property IDs and defaults; test stale paths and explicit CLI selection; document the changed activation authority.
- [Large mechanical application moves] → Keep algorithms unchanged and exercise Viewer, Editor document/view behavior and GPU validation after integration.
- [Over-generalization] → Use engine-owned domain interfaces and one real render-feature migration instead of generic per-function reflection or arbitrary threaded callbacks.

## Migration Plan

Implement and test the runtime first; add service plugins; adapt feature interfaces; migrate Viewer and Editor; enable render-stage consumers and build selection; run style/boundaries/naming, targeted and broader integration checks; document the resulting extension contracts. Keep the change and all code uncommitted for review.

## Open Questions

None required before implementation. Additional editor-panel and render-effect extraction can use the provided contracts without requiring runtime plugin loading.
