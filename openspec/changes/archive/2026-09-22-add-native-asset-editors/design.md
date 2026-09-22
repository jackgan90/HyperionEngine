## Context

The current Editor is one cohesive plugin with one scene document. Native assets already have stable IDs, typed immutable CPU loading, current references, atomic asynchronous saving, material resolution and GPU preparation. The remaining Catalog type duplicates the discovered index. Scene history currently stores scene nodes with resolved resource pointers, so saved dependency refresh must also prevent historical snapshots from restoring stale resources.

## Goals / Non-Goals

**Goals:** Open every current supported hasset type; support independent non-scene document tabs, real previews, useful read-only information and safe simple edits; independent undo/redo; typed reference replacement; refresh open consumers after save without losing authored state. Preserve established scene controls and native identity.

**Non-Goals:** Shader/material graphs, shader text editing, new asset formats, model topology/hierarchy restructuring, LOD/collision, image painting/compression, sky rebaking, bulk reference rewrites, filesystem watching or automatic commits.

## Decisions

### Remove persistent catalogs before adding editors

Remove FAssetCatalog and its reflection and CLI writer. Assets accepts discovered native references into an in-memory index with explicit duplicate/identity validation. Startup, root transitions, import, migration and tests use this API. Historical migration filtering may recognize obsolete management filenames without registering their former types. Keep archived records intact; update current documentation/specifications.

### Editor-owned document workspace

Add a workspace owned by the Editor plugin. Each document owns its ID/path/header, detached draft, history cursor/state, loaded baseline, async operations and preview state. Canonical asset ID deduplicates opened tabs within a content session; scene opening still uses the existing protected scene path. Commands target the focused document. Dirty close/root/exit protection covers all documents, including saves accepted on the old mount. A document epoch and edit state reject stale completions. Diagnostic documents never write unsupported data.

Use engine Gui wrappers for tabs and property controls; no direct ImGui dependencies in Editor. Keep individual asset adapters private to Editor rather than introducing a plugin per type. CPU authoring/validation helpers remain with their data modules. Runtime never depends on Editor. Existing Start/Update/Quiesce/Stop ownership, declared services and resource retirement remain mandatory.

Accepted tab closes are queued until the next Main update, before GUI submission. A preview submitted during the close click remains owned and bound through that frame's rendering. Scene placement cancellation only cancels the placement payload; it must not cancel GUI window docking/movement or another feature's drag operation when the scene viewport is hidden by an asset tab.

### Drafts and history

Separate authored fields from loaded dependency snapshots. Commands retain field deltas or immutable before/after values; large bulk data is shared across ordinary metadata edits. One active widget interaction forms one history transaction, applies preview changes immediately and supports cancellation. Save captures the current state and clears dirty only for that state. Navigation, selected mip/face, exposure, background and preview geometry are transient.

Material controls project typed values rather than exposing serialized word bit patterns. Existing editable numeric leaves, vectors/colors, textures and sampler values are supported; scope/policy constraints still apply. Parameter declarations, shader/pass structure, render-mode flags requiring coordinated pass rewrites and complex matrix layouts remain read-only. Reset removes a Values override and reveals the declared default. References come from discovered assets and validate type, dimension and graph before replacing a value.

### Shared preview mechanisms

Textures display selected mip/cube face and channels with correct linear/sRGB/HDR display conversion, pan/zoom, alpha background and pixel inspection. The only encoding authoring operation is RGBA8 Texture2D reinterpretation plus rebuilding lower mips from unchanged mip0 using the existing CPU builder; floating textures remain Linear and cube encoding is read-only.

Model, material and sky previews use private transient scenes and cameras, independent render sessions sharing the owner's resource service and the normal scene pipeline. Model preview preserves native materials; material preview applies the actual material to engine sphere/plane/cube geometry, including existing custom Forward compatibility passes. Sky preview supplies native radiance/specular/BRDF/SH to the scene and reference spheres. No temporary hasset is written. Hidden tabs skip drawing. Each preview has distinct view/pipeline/output identity and retains submitted resources until normal fence retirement.

Model edits cover names, local PRS preserving affine data, primitive assignment to existing slots and replacement of existing material-slot references. Stable node/primitive IDs, hierarchy and geometry arrays remain read-only. Sky name is editable; bake products, coefficients and convention are read-only. Exposure and orientation are preview settings, not invented asset fields.

The native asset host displays readiness in its always-present, fixed-height status bar before laying out the dockspace. Ready/preparing transitions during continuous property edits must not insert or remove rows above the preview image, resize its render target, or change its camera aspect ratio. The edit and asynchronous preview publication paths retain their existing behavior.

### Saved dependency refresh

Successful save produces a Main-owned publication record containing canonical identity and the saved state. Invalidate only changed CPU assets, refresh the discovered index entry and notify open consumers. Compute dependency closure using resolved references, including dependencies referenced by dirty drafts. Re-prepare affected resources asynchronously and install only current-generation results at a safe frame boundary. Preserve document authored state, scene handles/selection/transforms and local material overrides. Refresh is not an edit and cannot dirty another document or enter its history.

The reusable scene refresh API replaces resolved resource data on existing nodes rather than calling Load/Close. History restoration rebinds resource-bearing nodes to current published dependencies, preserving historical authored overrides instead of resurrecting old CPU/GPU pointers. Save completion and refresh failure are distinct statuses. Prior valid rendering stays available while replacement is preparing or fails. Ordinary material value changes reuse geometry; texture changes rebuild dependent material bindings without reimporting assets.

### Save correctness

Extend native saving with an optional expected ID/revision precondition checked in the serialized write operation. Keep same-path ID, portable current references, freshly derived dependency headers and existing provenance-clearing behavior. Read-only mounts, changed external files and invalid candidates report failure without clearing dirty. Saving does not wait on transient preview success, but reference replacements must be valid. No unconditional external-file overwrite or implicit Save As is introduced.

## Risks / Trade-offs

- Large assets can make encoding/conversion expensive: perform it on Workers, share bulk history and skip unchanged preview preparation.
- Multi-document close and root switching have admitted IO: keep old mappings alive until saves finish, cancel/join readers before destruction and reject stale results.
- Scene history contains derived data: rebind on restoration and test refresh followed by undo/redo, including material overrides.
- Generic materials do not all implement standard PBR: use declared parameters and actual passes, keep unsupported structures read-only and show actionable preview errors.
- Catalog removal changes public C++ and CLI interfaces: migrate all owned callers and update current contracts in the same change.

## Migration Plan

Current Engine/Game content contains no Catalog type or references, so no content rewrite is required. Remove the obsolete type/API usage, add workspace and preview helpers, then texture, model, sky and material adapters and cross-document refresh. Verify fixtures and real Editor input paths, build Debug/Release and run affected regressions. Leave the change and working tree uncommitted for user acceptance.

## Open Questions

None blocking. The user confirmed multiple asset tabs, individual existing reference replacement, and save-triggered automatic consumer refresh with unsaved scene state retained.
