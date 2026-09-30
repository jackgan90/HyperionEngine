## Why

Recent Editor interaction work established shared scene and asset documents, but callers still duplicate shortcut admission, asynchronous asset edits, hierarchy filtering and automation adapters. The resulting drift, query-based validation, implicit numeric contracts and oversized Editor ownership make cross-feature behavior harder to reason about and validate.

## What Changes

- Centralize scene shortcut admission using a current-frame interaction snapshot, preserving command-specific policies while preventing Delete from mutating scenes when another panel, popup, text input or gesture owns input.
- Move texture encoding and asset reference asynchronous edit orchestration into a shared CPU domain workflow used by Editor and automation, including generation checks, busy state, completion, failure and shutdown.
- Replace scene enumeration used only for validation with direct document/revision/handle checks; share ordered selection-root filtering and stop traversal when an already selected ancestor covers a node.
- Separate acceptance-test state and input routing from production Editor state; give viewport and document-transition responsibilities cohesive owners and split long mixed-responsibility functions at real boundaries.
- Derive import choices from stable capability type IDs and expose typed texture preview metadata instead of parsing display strings.
- Name engine input buttons/modifiers, automation budgets and units; consolidate private automation registration/error adapters while retaining per-operation metadata.
- Remove verified unused private compatibility wrappers and update stale active documentation and API descriptions to match implemented light priority and scene editing behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-selection-shortcuts`: shared input admission across scene shortcut commands, including delete and history behavior.
- `shared-asset-documents`: one asynchronous edit orchestration contract with stale completion and lifecycle protection for GUI and automation.
- `shared-scene-documents`: bounded validation work and shared ordered selection-root filtering without changing transaction semantics.
- `editor-application-consolidation`: cohesive production state, isolated acceptance driver and preserved optional-feature/lifecycle validation.
- `import-draft-preview`: typed texture inspection metadata and capability-driven GUI import choices.

## Impact

Affected areas are Runtime/SceneEditing, AssetEditing, AssetImport, Platform and Automation; Plugins/Editor and Automation; focused tests, CMake test selection and active documentation. Preserve public operation IDs, existing serialized fields, stable identities, error behavior, revision/history/persistence contracts, plugin lifecycle and module boundaries. DTO metadata additions are additive and reflected. No new dependencies, renderer feature changes, transport domain branches, clipboard/light behavior redesign or historical artifact rewrites are intended.

The orchestrator authors this plan and reviews results. All implementation and repairs are performed by an independent subagent with no inherited conversation, using gpt-6.1-sol at high reasoning. Delivery remains an active OpenSpec change with uncommitted work; do not archive or commit.
