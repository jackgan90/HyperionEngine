## Why

Native project content currently combines hidden versioned dependencies, pinned authoring references and an authoritative import library. This prevents edits to one asset from naturally reaching its consumers and makes project maintenance dependent on historical publication state. Project assets should have stable identities, one current file and portable, reconstructible discovery.

## What Changes

- **BREAKING** Publish dependencies into visible typed folders at stable paths; authoring references follow the current asset rather than a persisted revision.
- Reconstruct ID/path/type discovery from native files, without requiring Catalog.hasset or .asset-library.hasset.
- Retain optional portable per-root import provenance and stable product mappings; reuse existing shared resources and avoid machine paths in published assets.
- Preserve identity on same-target reimport, avoid writes for unchanged valid imports, allow distinct explicit root targets, and stage conversions before rollback-capable publication.
- Show every project hasset in Editor by default; keep Git, caches and publication temporaries excluded.
- Migrate every currently usable HyperionAssets resource, consolidate the known accidental public/hidden aliases, preserve cross-mount sharing, and remove obsolete internal generations and management files after verification. Do not resurrect missing historical records.
- Keep both repositories uncommitted.

## Capabilities

### New Capabilities
- `native-asset-registry`: Lightweight metadata discovery, duplicate identity diagnostics and reconstructible ID resolution.

### Modified Capabilities
- `native-asset-management`: Current-content authoring references and fresh graph reloads.
- `offline-asset-import`: Portable per-asset product identities, visible stable outputs and staged publication.
- `external-content-repository`: Current-only assets, optional source recipes and complete native migration.
- `editor-content-browser`: Ordinary visibility for all native project assets.

## Impact

Runtime IO, Serialization, Assets and AssetImport; application asset services; renderer persistence; Editor content browsing; AssetTool; importer/native/renderer/content acceptance tests; documentation and F:/HyperionAssets. Existing module and plugin lifetime boundaries remain unchanged. Dedicated non-scene editors, an Editor import dialog, automatic background hot reload and independent mesh assets remain future work; this change supplies their reusable asset foundation.
