## Why

Material sampler controls convert presentation indices directly into enum values. Reflected inspector choices also encode values through label positions, so reordering labels can change the value being edited.

## What Changes

- Represent inspector choices with explicit archive values and labels; share lookup and validation between ordinary and mixed inspectors.
- Bind material sampler controls to typed enum descriptions while preserving current labels and values.
- Replace positional material type labels with explicit enum mappings.
- Preserve domain validation, edit transactions, history, persistence and automation schemas.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `component-inspection`: Choice presentation order is independent of stored values, including mixed selections.
- `native-asset-editors`: Material sampler choices resolve typed identities rather than enum ordinals.

## Impact

Reflection presentation metadata, Gui inspector adapters, Editor material widgets, and existing Scene choice declarations. No new end-user capability, plugin lifecycle, dependency direction or persisted format.
