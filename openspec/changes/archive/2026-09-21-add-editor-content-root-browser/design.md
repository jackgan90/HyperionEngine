## Context

Editor currently reads catalog scenes once; IO mount sets are frozen, AssetService.Drain permanently closes admission, and graphics/GUI services borrow stable objects. A root change must therefore be an explicit quiescent transaction, not plugin reload or an unsynchronized path edit.

## Goals / Non-Goals

**Goals:** native folder selection; persistent last root and five recents; real tree/grid browsing; dynamic scene discovery; same-path A/B isolation; save/discard/cancel protection.

**Non-Goals:** project manifests, file mutation tools, asset import UI, editing non-scene assets, filesystem watchers, Git commit or automatic OpenSpec archive.

## Decisions

1. Keep plugin selection and service addresses stable. The assets plugin exposes an engine-owned content-root service, prepares a validated mount/catalog candidate, and commits after the Editor has released consumers. Mount sets remain frozen during requests; a Main-only exclusive replacement is allowed after producers have joined. Asset reset drains and clears all caches and catalogs before reopening admission. This explicitly extends the old construction-only mount contract without introducing runtime plugin lifecycle changes.
2. Root changes are queued by GUI and processed between frames. Validate the directory and mount overlap first; reject inaccessible/non-directory paths. Same canonical root is a no-op. Dirty documents offer Save and continue, Discard, Cancel; pending saves finish against the old mount and failures abort. Close scene, cancel/join browser and open requests, clear history/selection/preview/path state, release retained GUI/render frames, then retire content resource caches and GPU work before committing. The initial implementation accepts a short synchronous retirement barrier for correctness.
3. Preserve `/Engine`; all consumers use the same mounted filesystem. Preflight catalogs using the candidate independently; an absent catalog is legal, a malformed/conflicting catalog rejects the candidate before document destruction. Revalidate the candidate after any user prompt and before teardown. Candidate ownership is move-constructible but not move-assignable so dependent services always destruct before their IO provider. Successful mount commit has no fallible disk work. Preflight/save/cancel failures leave the old document intact; failures after teardown begins are reported as controlled application failures rather than rendering a partially closed scene.
4. Platform owns the native folder dialog adapter. Gui owns split-pane and file-tile drawing; Editor owns directory selection and action dispatch. No third-party GUI or Windows types enter Editor.
5. Browse immediate children asynchronously through directory entries, with lazy tree expansion. An independent cancellable recursive metadata scan supplies Open Scene; it decodes each bounded native file to validate TypeId without loading scene dependency graphs. Both paths use the same filtering and `/Game` namespace. Cancellation plus a join before replacement prevents stale completion publication. Navigation invalidates the selected listing between UI frames; Refresh and scene save invalidate all listings and restart scene discovery. Directory errors are local diagnostics, not silent fallback.
6. Default hide `.assets`, `.asset-library.hasset`, and `Catalog.hasset`; Show Internal Assets includes them. Exclude `.cache` and `.git` subtrees regardless of toggle. Other directories, including empty directories, remain visible. Only `.hasset` files appear; folders sort first. Links remain excluded by mount policy. Refresh and successful save update listings; no live watcher.
7. Double-click classifies the native header TypeId; scenes reuse OpenScene and its dirty guard, other valid types show unsupported, corrupt files show read/format failure. Open Scene recursively discovers current-root scenes rather than a hardcoded list or mandatory catalog.
8. Extend local Editor preferences compatibly with optional root/recent fields; explicit `--mounts` overrides restoration. Persist normalized successful roots, newest first, at most five; no failed selections are inserted. Invalid restored roots show an error and leave an unmounted Game workspace rather than silently selecting a different root. Existing automated exercise profiles do not consume interactive root history.

Display paths are separate from package identity: Open Scene strips the `/Game/` prefix for list labels and editable paths, translating relative input back to the Game namespace. Content Browser uses `All` for its root and breadcrumb prefix. Internal navigation, scene selection and IO keep the original package paths.

## Risks / Trade-offs

- GPU retirement may briefly block the UI → perform only on an explicit root switch, never per frame; retain normal fence ownership and exercise A/B/A transitions on D3D12.
- Large directory/metadata scans → asynchronous cancellable work, lazy listing, bounded reads and local errors; no eager graph loading.
- Old revisions share paths/IDs across roots → clear session caches and catalog mappings before new admission; test both CPU assets and rendered scenes.
- Internal assets are real dependencies → visibility filtering never changes IO resolution or deletes files.

## Migration Plan

Read existing version-1 preferences and preserve capture preference. Add optional persisted root fields. Keep native hasset formats and package references unchanged. Validate IO/assets, preferences/browser tests, editor graphical acceptance, plugin absence paths, style and module boundaries. Leave the change uncommitted and available for review.

## Open Questions

None; the user confirmed restoration, empty document after switch, internal visibility rules and dynamic Open Scene discovery.
