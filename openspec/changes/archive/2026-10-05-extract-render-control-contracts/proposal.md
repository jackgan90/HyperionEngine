## Why

Automation consumes renderer control interfaces whose public headers also expose pipeline construction, render snapshots and device/task sampling. Those implementation dependencies make reusable control consumers depend on the complete Renderer target.

## What Changes

- Establish a CPU RenderControls module owning the seven existing control interfaces, their data contracts, reflection and pure validation.
- Keep pipeline conversion, file persistence, device/task sampling and render view snapshots with Renderer; preserve compatibility includes for current renderer callers.
- Migrate Automation to the control contracts and verify public consumers using only the new target and its declared CPU dependencies.
- Preserve operation IDs, reflected schemas, defaults, revisions, provider lifecycle and GUI behavior.

## Capabilities

### New Capabilities

- `render-control-contracts`: CPU control contracts with explicit production adapters and dependency evidence.

### Modified Capabilities

None. Existing user operations retain their behavior.

## Impact

Runtime Renderer, the new Runtime RenderControls module, shared raster value contracts, Automation and current Editor/DebugUI consumers, target boundary checks, contract tests and ownership documentation. ApplicationServices still aggregates rendering services; removing Automation's direct Renderer edge does not establish a CPU-only application executable.
