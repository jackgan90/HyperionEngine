## Why

Editor is becoming the primary application, but scene nodes have mutually exclusive payloads, the model-to-render-primitive relationship lacks an inspection contract, and Details is hand-written and read-only. Editing, rendering and saving need one component-oriented authority with explicit transaction and cross-thread contracts, while model instances retain shared asset identity without copying internal topology into the scene.

## What Changes

- Introduce registered, identified scene components, including Transform, without ECS or a universal tick hierarchy. Allow independent capabilities to compose on one object.
- Extend record reflection with typed property inspection metadata and a generic Inspector that commits validated edits, supports undo/redo, and tracks save points.
- Keep one scene object per whole-model reference, with instance Transform and material overrides. Preserve stable model subresource identities for diagnostics and compatibility with explicitly expanded scenes; do not automatically copy model topology into scene objects.
- Publish immutable, versioned rendering diagnostics to Main; never expose Render-owned mutable objects to GUI.
- Separate editor navigation from authored scene cameras, and implement scene save/save-as, component/object editing and resource replacement.
- **BREAKING**: Advance native scene/model schemas with explicit migration, update import/rebuild recipes and migrate the shipped Sponza dependency graph while preserving scene appearance and asset identities.
- Deliver reproducible before/after Debug and Release performance evidence, including editor overhead, static/moving rendering, load/save and memory/workload counters.

## Capabilities

### New Capabilities

- `scene-components`: Component identity, registration, composition, transactions and persistence.
- `component-inspection`: Reflected property editing and immutable rendering diagnostics.
- `editable-model-instances`: Whole-model instance references, stable subresources, explicit expansion boundaries and asset graph migration.

### Modified Capabilities

- `scene-editor`: Editable Details, document history/save points and independent viewport camera.
- `renderer-cpu-benchmarks`: Reproducible component/editor iteration comparison.

## Impact

Reflection, Scene, Renderer, Gui, AssetImport, AssetTool, Editor, Viewer compatibility, tests and documentation; HyperionAssets native assets, catalog and source recipes. Runtime remains independent of Editor, native backends and third-party GUI APIs. No new external dependencies, ECS, automatic reimport merge, scripting runtime or git commit is part of this change.
