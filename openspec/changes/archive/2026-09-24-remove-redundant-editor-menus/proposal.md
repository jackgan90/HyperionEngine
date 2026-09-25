## Why

Acceptance of the application migration found redundant Edit menu entries. Scene role selection already has object-oriented controls, while duplicate and keep-children deletion need a future Outliner or viewport interaction design.

## What Changes

- Remove Edit > Scene settings, Duplicate primary object and Delete primary, keep children, including their unused UI methods.
- Preserve existing object controls, Details hierarchy editing, shared SceneEditing transactions and Automation operations.
- Update current documentation to distinguish available Automation operations from deferred GUI entry points.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-application-consolidation`: Migrated capabilities must fit existing Editor interaction surfaces without the redundant Edit menu entries.

## Impact

Editor private menu code, Editor and Automation capability documentation, and the consolidation specification. No new GUI replacement, runtime/API changes, or Git commit.
