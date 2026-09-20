## Why

Editor selection, property editing and history currently target one object. Scene layout and repeated component edits require a shared multi-selection with predictable group manipulation and one undo step per interaction.

## What Changes

- Add ordered selection with a primary object, Ctrl-click toggling in Outliner and viewport, and synchronized highlights.
- Show the intersection of selected component types with exact per-field mixed values; apply only explicitly edited fields to every target.
- Keep Details transform edits as absolute local assignments. Apply gizmo transforms to the group around the primary object, preserving its existing single-selection world result and avoiding hierarchy double application.
- Add atomic multi-node edits, grouped history, multi-subtree deletion and selection restoration.
- Define zero-scale and unmatched collection limitations, retain existing input interruption rules, and document/test the new interactions.

## Capabilities

### New Capabilities
- `editor-multi-object-editing`: Selection, shared inspection, group manipulation, atomic transactions and multi-object history.

### Modified Capabilities
- `editor-viewport-picking`: Ctrl-click selection policy.
- `scene-transform-gizmos`: Primary-object group manipulation and explicit multi-selection degeneracy limits.

## Impact

Editor plugin; Runtime Reflection/Gui inspection; Scene transactional mutation; Renderer SceneInstance and transform manipulation; editor/scene/reflection/renderer regression coverage and documentation. No new dependencies, plugins, persisted scene schema, dynamic lifecycle or Runtime/Application feature branches. No git commit or automatic OpenSpec archive is included.
