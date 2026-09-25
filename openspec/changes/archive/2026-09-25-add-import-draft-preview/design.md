## Context

The active import GUI change shares publication across GUI and automation. Publication already stages encoded products before Commit; native editors provide model/property validation but their Save path intentionally drops import provenance.

## Goals / Non-Goals

Goals: property-only preview of converted root assets, limited edits, history, no writes before confirmation, reproducible publication, equivalent automation.

Non-goals: GPU preview, editing generated dependencies, topology/geometry editing, shader or reference replacement, full scene editing, automatic merging after source changes, native schema changes, archive or commit.

## Decisions

1. Add asynchronous preparation to AssetImport. Reuse conversion and publication graph traversal without Commit to collect immutable root/products and all source fingerprints. Reuse these snapshots on publication rather than converting different data after confirmation. Final publication retains existing leases, identity allocation, deduplication, graph checks and commit/rollback.
2. Keep drafts in the existing Main-owned import workspace. Bound resident drafts and history. Store only typed property overrides in history, sharing bulk data. Dirty drafts participate in content-root checks; preparing work is busy; cleanup waits for workers before releasing snapshots. Closing the preview hides it; explicit discard releases its draft. Changes to source/options mark GUI results stale and require refresh; replacing edited drafts requires explicit discard confirmation.
3. Root edits: name for Model/Texture/Sky/Material, model stable-ID node names/local matrices and primitive names/material indices, allowed numeric material values. Existing model validation and material numeric helpers remain authoritative. Scene structure and generated child assets are read-only. Conversion controls remain in the parent panel and rebuild the draft.
4. Store canonical typed override data in import provenance Settings; no asset schema change. Empty overrides preserve old one-step behavior. A prepared draft is a new explicit import decision; no automatic inheritance/merge of prior output edits. Reusing identical requests/overrides retains freshness. Changed source fingerprints reject submit even with force or up-to-date outputs.
5. Typed automation operations prepare/get/edit/history/submit/discard use draft IDs and optimistic generations, distinct from import task IDs and session jobs. Queries page potentially large property lists. Transports remain unchanged. Editor uses identical operations directly.
6. The second non-modal GUI window presents summaries, paged node/primitive/material details and editable controls. It does not create a separate native window or GPU renderer. Existing affine decomposition preserves shear while exposing position/rotation/scale. Bulk fields and stable IDs never become editable controls.

## Risks / Trade-offs

- Preparation does graph staging work even when output is up-to-date -> retained cached conversion avoids repeating expensive baking; first version prioritizes correctness over optimizing dry traversal.
- Source/dependency edits after preview -> compare captured fingerprints before freshness checks and again at commit.
- Large assets/history -> bounded drafts and metadata overrides; no geometry in wire responses or history.
- Concurrent root/source/UI changes -> versioned drafts, explicit stale state, no silent edits applied to another source.
- Native editor save would lose provenance -> share validation helpers, publish exclusively through AssetImport.

## Migration Plan

Additive typed operations only. Preserve asset.import and existing CLI. Existing files stay valid. Keep both active OpenSpec changes unarchived and all code uncommitted.

## Open Questions

None blocking. Realtime rendered previews and editing generated material/texture dependencies remain future extensions.
