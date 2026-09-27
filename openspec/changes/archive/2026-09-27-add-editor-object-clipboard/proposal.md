## Why

Editor users cannot copy an ordered selection and paste independent objects from its captured state. The existing single-node duplicate operation does not preserve a selected subtree or follow the operating system clipboard when ordinary text replaces a copy.

## What Changes

- Add Ctrl+C/Ctrl+V for selected scene subtrees, preserving authored properties and generating collision-free numbered names and new identities.
- Capture immutable, document-scoped snapshots; use a genuine typed system clipboard token so text, files, images and other applications replace the object copy.
- Commit each paste as one atomic undoable creation batch with selection and handle restoration.
- Share copy, clipboard status and paste operations between Editor and typed automation.
- Define focus routing, resource/reference validation, budgets, content/document invalidation and controlled failure behavior.

## Capabilities

### New Capabilities
- `editor-object-clipboard`: Document-scoped multi-object clipboard, platform ownership, atomic paste/history, input routing and automation parity.

### Modified Capabilities
None. Existing single-node duplicate operations retain their contracts.

## Impact

Runtime SceneEditing, Scene, Renderer, Platform and Gui; Editor and Automation plugins; scene/history, platform, automation and Editor acceptance tests; Editor and automation documentation. No external dependency upgrades, application lifecycle changes, asset format migrations, clipboard export across documents/processes, archive or Git commit.
