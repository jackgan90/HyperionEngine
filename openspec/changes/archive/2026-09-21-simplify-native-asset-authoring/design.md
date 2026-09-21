## Context

Project hassets are authored native data, not disposable source-import caches. Current publication uses root files plus hidden immutable dependency generations. The user authorized engine development and migration of F:/HyperionAssets, without commits and without restoring absent historical imports. Runtime/Scene and Environment remain independent CPU modules; asset orchestration remains in the assets plugin.

## Goals / Non-Goals

**Goals:** stable asset identity; one current file per identity; current-content references; maximal reuse of explicitly shared resources; portable reimport metadata; discovery without mandatory management files; visible native assets; native-only migration and verified reload/render behavior.

**Non-Goals:** non-scene asset editing UIs, Editor Import dialog, automatic background hot reload, independent mesh asset extraction, historical schema cleanup unrelated to this change, restoration of absent source or historical assets, commits or push.

## Decisions

1. Persist AssetId, TypeId and a package/relative path hint in authoring references; clear revision constraints when publishing/saving authoring data. Keep the header content digest and low-level explicit revision validation for diagnostics/tests. Existing active objects remain immutable until released. Reloading a scene refreshes native input caches; live file watching is not implied.
2. Build an in-memory registry from lightweight native metadata. Add range reads to engine IO and metadata-only archive decoding so startup does not read texture/geometry bulk. Validate integrity fully on actual load. Exclude .git, .cache and publication state; diagnose duplicate IDs rather than choosing arbitrarily. Application startup and root switching discover registered types without Catalog.hasset. Path references continue to work without any persisted index; ID resolution also survives asset moves after rebuilding discovery.
3. Replace the persistent library index with discovery of existing native files and their optional root provenance OutputIds. Ignore mappings whose target ID no longer exists. Same-target reimport retains the target ID and child mappings. An explicitly new root target receives a new ID. Portable source roots are logical names; the default source namespace is stable per imported target and relative to the selected source directory. Never persist local absolute paths, including opaque identity keys. Optional explicit source IDs support cross-root source associations; generated texture candidates may also reuse equivalent native content. Distinct editable material sources keep identity.
4. Publish dependencies under visible Models, Materials, Textures, Skies or Assets folders, using readable names with stable ID suffixes for collision avoidance. Resolve an existing ID to its current path before creating a new file. Header digests never appear in filenames. Shared resources are updated at their current path, so parents need no rewrite solely for a dependency content change.
5. Convert and validate the whole import graph before writing. Serialize library publication through a non-hasset lease; stage encoded files in memory, skip identical bytes and retain previous bytes for rollback of synchronous publication failures. Remove newly created files during rollback. Held CPU/GPU snapshots remain valid. Import is an authoring operation; it requires a quiescent publication boundary for concurrent readers, and does not claim cross-process multi-file atomicity or power-loss recovery.
6. Keep the current generic browser/type dispatch, remove the internal-assets toggle and show all project hassets. Non-scene types keep the controlled unsupported-type response pending their editors.
7. Migrate native data directly with AssetTool, preserving authored scene edits. Traverse every public native root and its dependency closure, choose public aliases for duplicate payloads of the same type, rewrite references and portable provenance mappings, publish the complete candidate tree, and validate/render against isolated mounts before replacing tracked content. Shader text and license metadata remain. Optional sample recipes stay available, with outputs updated to the new structure. No missing historical mapping is recreated.

## Risks / Trade-offs

- Updating a shared resource affects all consumers -> preserve per-instance overrides and test two-parent reloads; explicit independent roots remain possible.
- Editable identity cannot be derived from content hash -> IDs persist through edits; fingerprints are only reuse/invalidation data.
- Conflicting source products may already share a native ID after content reuse -> reject the entire publication before writing if they now request different contents. Automatic splitting of shared identities is outside this change. Reuse candidates must match actual staged content, and reused products participate in this conflict check.
- Import freshness, external native graph validation and single-root tools use the same discovered ID resolution as runtime. Mounted tools search all configured roots; unmounted single-file tools search the input parent directory.
- Metadata-only discovery does not verify all payload bytes -> full native integrity validation remains mandatory at load.
- Import provenance can become stale after native authoring -> native saves remove provenance as before; discovery never treats provenance as runtime authority.
- Filesystem write failure during rollback can itself fail -> report the original and rollback errors; migration keeps an untouched native backup before external publication.
- Existing pinned schemas/fixtures conflict with current authoring semantics -> update affected contracts and targeted fixtures; explicit low-level pinned-reference tests remain valid.

## Migration Plan

Capture original tracked content and root graphs under ignored engine out; build migration and new importer; stage current-only content; validate every staged hasset, aliases, cross-root references and forbidden machine/internal paths; compare scene/model/sky rendering and persistence; publish the verified tree into HyperionAssets and remove only enumerated superseded tracked native/management files. Validate both final worktrees and leave them uncommitted.

## Open Questions

None blocking. Future asset editors, import UI and watcher can use the same native registry/import services.
