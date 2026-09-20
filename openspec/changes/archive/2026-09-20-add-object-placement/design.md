## Context

Editor owns Main-thread documents/history and uses engine-owned Gui, SceneInstance and SceneRenderPipeline interfaces. CPU scene ray queries and transform gizmos already exist. Gui image focus rejects unrelated active items, so source-to-viewport dragging needs an explicit target contract. SceneInstance currently loads an immutable manifest and rejects model IDs outside its existing load table; Snapshot reads references from that manifest. Engine Content currently has no standard primitive model library. Only the selected MainDirectionalLight contributes directional illumination.

## Goals / Non-Goals

**Goals:** deliver the confirmed eight object types, many-to-many categories, continuous world-space preview, transactional creation, save/reload, light icons and reusable placement contracts. Support the initially empty editor document and preserve plugin, thread and resource ownership.

**Non-Goals:** arbitrary asset browsing UI, native OS file drops, runtime plugin loading, terrain brushes, physics placement, parametric shape editing, multiple active directional lights, local-light shadows, Favorites/Recent, or Git submission.

## Decisions

### Palette and factories

Keep the feature in the cohesive Editor plugin. Expose an engine-owned registry with stable type/category IDs, a category list per type, display metadata, model/icon references and a creation factory. Prepared model data supplies placement bounds. All is a derived deduplicated view; Basic overlaps Shapes/Lights. Factories produce CPU node candidates; UI never switches on category to instantiate a type. Registration validates duplicates/references and can be removed by a contributing plugin's scope. The editor composes independent registry, preparation, drag-session, panel, overlay and document-commit helpers.

### GUI input ownership

Add copied, engine-owned drag payloads and source/target delivery operations to Gui. Use the private ImGui adapter for native drag ownership and hover rules, not the ordinary viewport focus gate. A source emits a stable type ID; a per-editor placement session owns that descriptor identity and document epoch, resolving the registry entry on each update so removal cancels safely. Route placement before gizmo/camera/picking. Release on a valid viewport is the only commit; Esc, focus loss, hidden viewport, modal UI, scene replacement and shutdown cancel. Leaving the viewport hides world preview while keeping the source gesture alive. Resizing or scale changes cancel a stale gesture safely.

### Placement geometry

Renderer exposes UI-independent ray/placement helpers. Query current Main scene geometry with the current viewport camera and pipeline filtering. Return a world geometric triangle normal derived from transformed triangle vertices, including mirrored/nonuniform/singular transforms. Hit positions use support distance from local bounds along the oriented hit normal; no automatic rotation in v1. A confirmed miss falls back to Y=0 then a camera-facing plane at the starting focus distance. Unavailable/incomplete intersections cannot commit. Coordinates use logical image bounds for input and actual framebuffer dimensions for the camera aspect.

### Engine resources and dynamic instantiation

Publish five deterministic native Engine models and a shared default material through existing asset tools; keep generation recipes in the engine repository. Model geometry includes normals, UVs and stable IDs. Cube edge, sphere diameter, cylinder/cone height and diameter, and Plane extent use one scene unit. Plane lies in XZ with +Y normal.

Keep an editable scene asset-reference table independent of the loaded immutable manifest. A public SceneInstance registration/preparation operation resolves full native references through Assets, deduplicates compatible identities, reports pending/ready/error, and shares model/query resources. AddNode then consumes that ready model ID. Snapshot emits only references used by live nodes. Undo may leave unused prepared assets cached; this is not an authored scene change. Empty documents can register resources and Save As without a source manifest. Load/Close cancel and join preparations, clear references and invalidate old document/attachment epochs.

### Transient preview rendering

Preview data never enters the authoritative scene, Outliner, scene ray query, light lists or undo history. Main freezes an owned per-viewport preview packet. Renderer uses an IRenderFeature before tonemapping for a real shaded mesh with scene depth testing, independent of scene light availability; the preview does not cast shadows or illuminate the scene. The feature receives immutable per-frame data through an explicit viewport input, not mutable Editor state or service lookup. GPU geometry/resources are shared or cached across drag frames; movement changes transforms only. Graph passes declare all color/depth/buffer reads and writes. The same CPU model resource is used for the final object; retain the final preview until publication is ready to avoid a visual gap.

### Icons and picking

Generate transparent PointLight (bulb), DirectionalLight (sun), and SpotLight (lamp/cone) images in one visual style. Keep source images and prompts in Content and import native textures for runtime use. Icons project world origins to the viewport and use stable screen size scaled with Gui. Extend clipped image overlays for textured icons, reuse the GuiRenderer texture-binding path, and render directional cues separately from the sprite. Icons are editor-only and always visible over geometry in v1, like existing transform gizmos; cull behind-camera/out-of-frustum markers. Add a visibility toggle. Hit testing uses the same clipped bounds as drawing; gizmos and active placement retain input priority, then visible light icons, then scene triangles. Disabled objects have no active markers.

### Commit and history

Resolve resources before allowing a model drag. Successful drop validates the epoch and candidate, adds one root node, optionally assigns the first directional light as MainDirectionalLight, selects the object and records one history entry including before/after settings. Factor existing camera creation into the common creation path. Failure rolls back node/settings and leaves selection/history/document state intact. Cancel preserves the redo branch. An existing main directional light is retained; Details offers an explicit undoable Set as main directional light action. Save As is enabled for valid empty/untitled documents.

## Risks / Trade-offs

- Asynchronous resource failures or late results → disable affected palette entries with useful errors, retain epoch checks and join tasks during shutdown.
- Drag/drop focus differs from click focus → target acceptance uses explicit GUI drag APIs and dedicated normalized-input regression tests.
- Preview resource lifetime crosses Main/Render/RHI → immutable packets and retained resource leases, graph-declared access and GPU validation.
- Always-visible markers can overlap → nearest-depth stable tie selection, viewport clipping and a visibility toggle; depth-aware markers can extend the representation later.
- CPU geometric placement does not reproduce shader deformation/alpha discard → preserve and document the existing scene-query limits.
- Asset generator or image output drift → ship native assets, source recipes/prompts and verify no-change regeneration for deterministic models.

## Migration Plan

Additive interfaces and Engine content only; existing scenes retain their archive format and original behavior. Existing layouts remain intact; reset creates the optional left placement dock. A disabled editor does not instantiate placement resources. Retain the completed change unarchived for review, with no Git commit.

## Open Questions

None blocking implementation. The user approved the initial behavior and authorized implementation without committing.
