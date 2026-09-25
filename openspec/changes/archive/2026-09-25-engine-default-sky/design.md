## Context

Cloudy is a native sky with two Game texture dependencies and the shared Engine BRDF LUT. SceneDocument already owns component edits, settings and undo/redo. AssetWorkspace publishes independent transient preview scenes. Both repositories began clean.

## Goals / Non-Goals

Goals: self-contained Engine sky; preserve IDs and authored lighting; explicit scene UI and automation parity; default Model/Material preview sky; updated reproducible source metadata.

Non-Goals: automatic sky injection into every scene, renderer algorithm changes, plugin lifecycle changes, archive or commits.

## Decisions

- Publish `/Engine/Skies/Cloudy.hasset` and `/Engine/Textures/CloudyDaylight{Radiance,Specular}.hasset`. Preserve IDs, bake payloads and provenance; regenerate native headers/revisions with the engine codec after path edits. Reuse the existing Engine BRDF. Copying only the sky would leave an invalid Engine-to-Game dependency.
- Put the canonical default reference in Environment. Add `UseDefaultSceneSky` to SceneEditing and `scene.sky.use_default` using reflected existing document/revision input and document-info output. A toolbar action calls the same function. Modify the active environment component in one history transaction, preserving orientation/intensity; enable sky visibility and the node. When absent, create one environment and assign its settings in the same transaction. Stale/busy requests use existing checks. Rendering readiness/failures retain the existing asynchronous scene diagnostics contract.
- Extend CommitCreate with explicit environment assignment for that operation, defaulting off for other creation paths. Avoid two independently undoable create/settings commands.
- Model and Material preview environment nodes use the same default reference. Keep preview key lighting and existing navigation; sky loading follows SceneInstance ownership and diagnostics. No authored asset references are changed by preview lighting.
- Migrate all native references by identity, retaining authoring provenance. Update the source manifest's Sponza recipe and remove the Game Cloudy output so rebuilding cannot restore the duplicate. Record the Cloudy source/license in Engine metadata.

## Risks / Trade-offs

- Missing custom Engine content can fail sky loading: retain explicit scene/preview diagnostics; no sibling fallback.
- Cross-repository migration can leave dangling references: stage rewritten native files, validate both mounted graphs and only remove the exact migrated originals; retain uncommitted Git originals for recovery.
- Lighting appearance changes in previews: verify real GPU render readiness for both asset types and preserve preview isolation.

## Migration Plan

Preserve native identities and texture bytes; relocate three assets and rewrite all referencing native objects, including headers. Update metadata and validate Engine-only and combined libraries. Rollback is possible from the uncommitted tracked originals and removal of added Engine files. No commits or archive.

## Audit repair decisions

- Source fixture preparation reads both Game Sources.json and Engine DefaultSkySources.json. The Engine source cache is explicit (`out/DefaultSkySources` by default); missing inputs fail with recovery instructions. It does not restore Cloudy publication under Game.
- Add an optional `RootId` to the shared import service, exposed as AssetTool `--root-id` and automation `asset.import.rootId`. Default behavior is unchanged. The option fixes a reconstructed root identity, requires a matching existing readable output, validates identifier syntax, and rejects another owner in the publication library before writes. The Engine manifest supplies the canonical Cloudy ID. Dependency product IDs continue to follow existing library reuse rules; existing scenes refer to the stable sky root.

## Open Questions

None.
