## Why

The Editor currently treats culling and selection-outline combo positions as protocol values and converts those values directly to enum ordinals. Reordering a label list can therefore change the selected behavior. This stage makes those relationships explicit while preserving the existing viewport service and external contracts.

## What Changes

- Define Renderer-owned culling and outline option descriptions linking existing typed identities, fixed wire values and display labels.
- Reuse those mappings in GUI selection, viewport state conversion and shared input validation.
- Verify fixed protocol examples, reordered/relabeled presentation, invalid input without side effects, real GUI choices and automation compatibility.
- Keep this as a focused commit checkpoint; profiling masks, wider runtime-state typing, configuration migration and SceneBridge keys remain later work.

## Capabilities

### New Capabilities
- `viewport-option-contracts`: Explicit culling and selection-outline option identities and presentation-independent boundary mappings.

### Modified Capabilities
None. Existing view operation shapes, lifecycle and rendering behavior remain unchanged.

## Impact

Renderer option contracts and viewport validation, Editor view controls/service adaptation, focused CPU/GUI/automation regressions, and developer guidance. No new module dependency, plugin lifecycle, operation ID, shader encoding or persistence format is introduced. The temporary maintainability draft remains outside Git commits.
