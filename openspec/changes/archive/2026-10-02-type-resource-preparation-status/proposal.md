## Why

Placement polling infers pending work from a `Preparing` message prefix, and asset previews infer sky failure from a formatted `Failed:` message. Changing display text or returning an error with a colliding prefix can change control flow; explicit preparation states make these decisions independent of presentation.

## What Changes

- Return typed placement preparation state, stage and independent error information from the shared placement service, including model, preview material and icon admission conditions.
- Use the same typed result for GUI admission, asynchronous automation polling and final placement validation.
- Make sky loading/uploading/terminal states explicit and let preview and light-diagnostic consumers inspect those states directly.
- Preserve sky status strings and existing reflection schemas through a domain-owned string codec; retain placement operation IDs, error codes and task completion semantics.
- Add regressions for message collisions, preparation stages, wire compatibility, recovery and GUI/automation equivalence.

## Capabilities

### New Capabilities

- `resource-preparation-status`: Explicit resource preparation states independent of display messages, with shared placement admission and compatible sky diagnostics.

### Modified Capabilities

None. Existing placement and sky behavior, serialization and lifecycle guarantees remain in effect.

## Impact

Editor private placement services and resource adapters; Renderer sky preparation and reflected lighting diagnostics; asset preview consumers; placement, sky and attached automation tests. No new transport branches, plugin dependencies, asset formats or native backend requirements.
