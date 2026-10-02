## Why

Scene component registration is extensible, but incremental changes only compare a fixed list of model, camera and light accessors. New registered components are visible only as generic metadata, forcing consumers to rediscover which instance changed and losing removed-instance identity. M07 closes this extension gap while preserving existing incremental rendering and spatial-query behavior.

## What Changes

- Publish owned component type and instance identities with Added/Modified/Removed occurrence flags through the existing scene change stream.
- Compare registered live instances using their descriptor equality, including repeated types, identity changes, node removal, initial synchronization and unacknowledged merges.
- Derive existing built-in compatibility classifications from the same differences, keeping hierarchy-derived transform/enabled effects and query invalidation explicit.
- Preserve atomic transactions, revision/acknowledgement, Undo/Redo and renderer-independent Scene ownership; document the consumer contract.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `scene-management`: Extensible, owned incremental component facts and compatible atomic propagation.

## Impact

Runtime Scene change values and private mutation preparation, focused scene/history/bridge/query tests, and SceneComponents documentation. This is a C++ incremental-data extension, not a new user editing operation or automation wire schema. Existing GUI and agent scene transactions obtain the same facts from the shared domain operation. No ECS, new synchronization consumer, renderer policy in the registry, persistent-format change, or plugin lifecycle change is introduced.
