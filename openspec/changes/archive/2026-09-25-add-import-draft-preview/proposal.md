## Why

Import settings cannot currently inspect or adjust converted asset properties before publication. Users need a read-only preparation stage and a small editable draft whose confirmed contents become the published asset.

## What Changes

- Prepare unpublished import snapshots, inspect type-specific properties, edit a restricted root property set, undo/redo/reset and publish or discard.
- Open a separate property preview after Source selection; keep conversion settings in the import panel and mark changed settings stale until refreshed.
- Preserve provenance, freshness, source fingerprints and transactional publication; expose equivalent typed automation operations.
- Reuse native property validation. No GPU preview, dependency editing, scene editor, new native types or transport changes.

## Capabilities

### New Capabilities
- `import-draft-preview`: unpublished converted drafts and property editing shared by Editor and automation.

### Modified Capabilities
None. This extends the active add-editor-asset-import change while preserving the existing one-step import contract.

## Impact

Runtime/AssetImport preparation/publication and workspace, shared AssetEditing validation, Editor import windows, Automation operation registration and acceptance tests. No archive or commit.
