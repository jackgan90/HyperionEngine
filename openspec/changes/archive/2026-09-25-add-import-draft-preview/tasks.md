## 1. Preparation and publication

- [x] 1.1 Add immutable prepared imports, complete source tracking and snapshot publication.
- [x] 1.2 Add restricted typed root edits, shared validation and canonical provenance.

## 2. Shared drafts

- [x] 2.1 Add bounded versioned drafts, inspection, history, submission and root lifecycle.
- [x] 2.2 Register typed automation draft operations without transport changes.

## 3. Editor

- [x] 3.1 Add automatic property preview with type-specific fields and stale/refresh handling.
- [x] 3.2 Add editing, undo/redo/reset, discard protection and confirmed submission.

## 4. Validation

- [x] 4.1 Add CPU and automation coverage for no-write preparation, edited publication, freshness and lifecycle.
- [x] 4.2 Exercise actual GUI preview/edit/history/import behavior and provider independence.
- [x] 4.3 Update documentation, run builds and required checks; leave changes unarchived and uncommitted.

## Validation evidence

- Debug and Release builds passed for Editor, AssetTool, automation CLI and import workspace tests. Final GUI changes were rebuilt in both configurations.
- CPU draft checks cover no-write preparation, metadata, name edits, optimistic generations, dirty protection, undo/redo, provenance, zero-write repeat publication, source fingerprint rejection with force, and root invalidation.
- JSONL and MCP draft acceptance passed for texture, model, model-to-scene, native scene, standalone material and sky. Coverage includes discovery/schema, paging validation, restricted edits, history/reset, immutable IDs, edited publication, identical-repeat freshness, dependency changes, and discard.
- Actual GUI input exercised source selection, automatic preview, name editing, undo/redo, publication, repeat zero writes and shared inspection by an attached agent. Dirty drafts reject direct/save close and permit explicit discard close. The same GUI workflow passed with automation providers disabled. The final captured layout was visually inspected.
- Debug: 16 related regression tests passed, including fixture setup, native asset editors, root transitions, automation transport/parity, plugin absence paths, GUI controls/docking, glTF import and native publication/CLI. Final GUI acceptance passed again after the final UI error-path fix.
- Release: asset_import_workspace, editor_asset_import and import_draft_automation passed (3/3).
- Full source formatting/path checks and module boundary checks passed. Semantic naming passed for all 35 affected translation units, with the final three GUI units rechecked. Both active OpenSpec changes passed strict validation; git whitespace checks passed.
- Local evidence: out/DraftRegression.log, out/DraftFinalGui.log, out/DraftReleaseAcceptance.log, out/DraftStyle.log, out/DraftBuild.log, out/DraftReleaseBuild.log, out/DraftFinalDebug.log, out/DraftFinalRelease.log.

At implementation completion, both changes were left active and uncommitted as requested. They were subsequently synchronized and archived on 2026-09-25 under separate user authorization.
