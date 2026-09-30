## Context

`FEditorPlugin::FrameScene` frames an independent `FSceneCameraView` through Runtime/Renderer `FitSceneCamera`. Viewport picking and Outliner use the same `FSceneEditDocument::Selection`; the last selected handle is primary, but framing must use the entire selection. Home currently lives inside viewport-only camera routing. F is absent from the engine key enum and its SDL/ImGui adapters.

`docs/PluginSystem.md` and `docs/Automation.md` require feature ownership in Editor, reusable algorithms in Runtime, and one UI-independent service for GUI/agent validation. Scene and SceneEditing must remain independent of Renderer. This change does not change plugin selection or introduce asynchronous work.

## Goals / Non-Goals

**Goals:**
- Frame selected world bounds with the current orientation/FOV and viewport aspect ratio.
- Handle multiple objects, selected subtrees, affine transforms and non-geometric nodes.
- Route F from Viewport, Outliner and Details with existing input-ownership rules.
- Share the operation through `ISceneViewport` and typed automation.
- Keep document state/history unchanged and remove the Frame Scene toolbar button.

**Non-Goals:**
- Animated camera transitions, new selection gestures, authored-camera changes, orthographic cameras or asset-preview shortcuts.
- Changes to transport, plugin lifecycle, content roots or assets.
- OpenSpec archive, spec synchronization into main specs, Git staging or commit.

## Decisions

### Reuse bounds fitting in Renderer

Extract the final bounds-to-camera calculation into a reusable `SceneNavigation` overload. Preserve whole-scene filtering and the reusable overload's existing default clip-plane policy. Editor explicitly fits temporary clip planes for both selection and whole-scene framing, so F followed by Home cannot retain clipping sized for a different target. Validate the complete candidate before publication and reset held navigation after success. Use finite calculations and stable minimum sizes; malformed geometry must not partially change the camera.

The existing bounding-sphere fit and 1.12 padding are intentionally retained. A camera-space tight box fit would change the established framing style and is unnecessary for this capability.

### Resolve current selected subtrees on Main

The shared provider snapshots the current selection and passes de-duplicated selected roots to a Renderer bounds resolver. Traverse descendants iteratively, validating handles before use. Models use `SceneModelBounds` plus their node world transform, preserving SourceNode/SourcePrimitive semantics and authored affine transforms. Selected hidden/disabled models remain navigable using their authored geometry. Existing hidden-section bounds semantics are retained.

Non-geometric leaves use a world-position-centered box with half extent 0.5 scene units; point/spot light influence ranges do not drive navigation distance. Groups with descendants use their descendant bounds without adding their own unrelated origin; empty groups use the same fallback. A loading model returns busy; terminal resource failure can fall back to its origin. Overlapping selected subtrees contribute each node once. Empty selection is a documented no-op and does not frame the scene.

### Publish a shared viewport operation

Extend `ISceneViewport` with a default unsupported selection-framing operation accepting the existing reflected `FSceneMutationRequest`. Editor validates current document/revision, idle/readiness and browsing-view availability before resolving bounds and committing the temporary camera. The operation also rejects active camera navigation and ordinary popups; these viewport-specific guards do not change the global document busy policy. Both F and `view.frame_selection` call this method. Automation reuses `FSceneViewportState` as result, stable owner cleanup and the existing typed operation catalog; no transport branches are added. Missing viewport/document providers remain explicitly unavailable. Existing operation IDs and schemas are unchanged.

### Route scene shortcuts after GUI ownership is known

Add F at the end of the engine enum to preserve existing values, and map it in SDL and ImGui private adapters. Only unmodified, non-repeat key-down is accepted. Allow Viewport/Outliner/Details focus, require a visible initialized viewport, and gate text ownership for the entire frame, popups, placement/reparent/gizmo/camera gestures, blocked document transitions and application focus loss. The camera update and viewport overlays must observe the same camera for the resulting rendered frame.

Home continues to frame the full scene through the existing shared method, and `view.frame_scene` stays supported. Remove only the main toolbar Frame Scene button; document F/Home in viewport guidance.

## Risks / Trade-offs

- Large world-space AABBs can give generous padding after rotation/shear → use existing affine `TransformBounds` and fit the resulting bounds conservatively.
- A group origin can be far from its geometry → only descendant bounds contribute when descendants exist.
- Non-geometric nodes have no physical size → use one documented stable fallback independent of light range and current camera distance.
- GUI focus or text deactivation can allow accidental shortcuts → reuse full-frame text ownership and add real GUI exercises.
- A malformed/stale or busy request can otherwise partially move the camera → resolve and validate into a candidate, then publish once.

## Migration Plan

No asset or configuration migration is required. Existing Home and automation clients retain their contracts. Validate Debug/Release builds, renderer/GUI/Editor acceptance, real CLI/MCP discovery/invocation and absence, style/boundary checks and strict OpenSpec validation. Record completed tasks and validation in this active change without archiving or committing.

## Open Questions

None for the approved initial scope. The fallback extent and conservative bounds policy are explicit defaults covered by regression tests.
