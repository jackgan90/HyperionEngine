## Why

Scene component edits enforce immutable model-source fields using a string list, while GUI candidates rely on inspector metadata and bypass that preparation path. Changes to field policy can therefore require separate GUI and automation updates despite their shared document history.

## What Changes

- Give SceneEditing one authoritative immutable-field policy bound to reflected C++ members.
- Validate GUI and automation component candidates through the same policy and derive contextual GUI read-only presentation from it.
- Preserve canonical reflection descriptors, operation IDs and schemas, resource validation, atomic transactions and continuous GUI history merging.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `automation-capability-parity`: Require canonical scene component field policy and shared candidate validation across GUI and automation.

## Impact

Runtime/SceneEditing and Editor component inspection, with CPU and automation/GUI parity tests. No component feature, persistence migration, reflection capability expansion, transport changes or other editor transition refactor.
