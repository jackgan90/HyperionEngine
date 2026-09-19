## Why

Viewer and Editor increasingly own optional feature lifetimes, input routing and concrete plugin coordination. The static registry cannot express service requirements, independent startup failures or early capture ordering, so removing an optional feature can terminate unrelated functionality.

## What Changes

- Extend the static plugin runtime with validated startup plans, required/optional service contracts, ordering constraints, explicit disablement, diagnostics, Main updates and scoped cleanup.
- Introduce a small application loop and reusable plugin-owned asset, window, graphics and GUI services. Compose Viewer and Editor as application feature plugins while keeping executable names and CLI contracts.
- Replace concrete scene-plugin routing with engine-owned extension interfaces and move feature-specific input/time handling into plugins.
- Integrate RenderDoc into the same lifecycle graph; retain immutable Main/Render/RHI snapshots and drain-before-release ordering.
- Add typed scoped event/extension registration and render-feature stages with explicit resource contracts; migrate contact shadows as a concrete render-feature consumer.
- Separate persisted feature parameters from activation. **BREAKING**: stale model/scene paths no longer reactivate disabled plugins; missing optional plugins are diagnosed and skipped instead of aborting unrelated functionality.
- Add build selection for optional feature plugins and document startup-only selection, service ownership and extension authoring.

## Capabilities

### New Capabilities
- `plugin-application-host`: Main-owned lifecycle/update host, typed services, scoped contributions and plugin-owned application composition.
- `render-feature-extensions`: Named render stages, resource-bearing contexts and optional feature registration.

### Modified Capabilities
- `configuration-and-plugins`: Explicit disablement, dependency/service planning, startup diagnostics and independent failure policy.
- `render-graph-plugins`: Generic scene/input extensions and plugin-owned rendering application services.
- `renderdoc-frame-capture`: Unified early lifecycle ordering and unavailable compiled-provider diagnostics.

## Impact

Runtime/Plugins, a small Runtime/Application module, service and application plugins, Viewer/Editor entrypoints, Renderer extension contracts, configuration, CMake selection, tests and architecture documentation. CPU Scene/Environment boundaries and native-backend isolation remain intact. No dynamic loading, runtime activation/deactivation or hot reload is introduced. Implementation remains uncommitted for review; archival and Git commit require explicit follow-up authorization.
