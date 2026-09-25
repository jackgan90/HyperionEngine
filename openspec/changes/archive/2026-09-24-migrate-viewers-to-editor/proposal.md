## Why

Editor is the maintained authoring application. The early standalone scene/model inspection applications duplicate composition, settings, diagnostics and acceptance paths and force unrelated features to maintain obsolete hosts.

## What Changes

- Complete shared scene structural history and expose duplication, keep-children deletion, reparenting and scene selections through Editor and Automation.
- Expose reusable rendering, visibility, bounds, shadow, capture and profiling controls through Editor; retain the existing asset editor and navigation implementations.
- Migrate rendering acceptance and benchmark entry points before removing their old hosts. Keep runtime pipeline and resource lifetime coverage independently testable.
- **BREAKING**: remove the standalone inspection plugins/application, their build options, experimental configuration, private adapters and documentation. Editor becomes the normal graphics application. AssetTool and standalone Automation remain.
- Preserve generic automation identifiers and schemas where semantics remain compatible; explicitly retire application-only configuration rather than silently reinterpreting it.

## Capabilities

### New Capabilities
- `editor-render-diagnostics`: Shared rendering controls, viewport debug geometry, statistics and profiling integration.
- `editor-application-consolidation`: Single maintained graphics application, migrated acceptance and removal of obsolete composition.
- `scene-camera-navigation`: Preserve shared Runtime navigation contracts independently of the retired application.

### Modified Capabilities
- `application-platform`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `automation-capability-parity`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `bounded-cpu-frame-pipeline`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `cascaded-shadow-maps`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `clustered-local-lighting`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `code-style`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `configuration-and-plugins`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `contact-shadows`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `deferred-render-pipeline`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `depth-conventions`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `editor-frame-capture`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `forward-render-pipeline`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `gltf-pipeline-validation`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `gui-texture-rendering`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `live-application-automation`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `mounted-content-filesystem`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `offline-asset-import`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `plugin-application-host`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `render-graph-plugins`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `render-primitives`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `renderdoc-frame-capture`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `rhi-presentation`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `runtime-module-organization`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `runtime-performance-profiling`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `scene-browsing-views`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `scene-cameras`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `scene-lights`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `scene-local-lights`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `scene-runtime-instance`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `scene-viewer`: Retire independent scene inspection application contracts after migrating reusable functionality.
- `scene-viewer-camera-motion`: Move reusable controller contracts into scene-camera-navigation.
- `selection-outlines`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `shared-scene-documents`: Undoable resource-preserving duplication and deletion preserving child world transforms.
- `static-model-rendering`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.
- `visual-studio-workflow`: Update supported host ownership and validation scenarios while preserving shared runtime contracts.

## Impact

Runtime SceneEditing/Renderer/Config/Core, Editor and Automation plugins, application composition, CMake and IDE setup, renderer/GUI/automation integration tests, profiling tools and active documentation/specifications. Existing native assets and formats remain compatible. No Git commit is part of delivery.
