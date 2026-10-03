## Why

AssetImport preflight and Environment baking independently encode the same sky size/sample constraints. Changes to those rules can make accepted imports fail later or cause the two entry points to accept different settings.

## What Changes

- Give Runtime/Environment authoritative bake limits and a reusable settings predicate; import preflight and baking consume it.
- Reuse the same specular/sample constraints for standalone prefiltering without adding the panorama radiance-size cap to captured cubes.
- Preserve legal ranges, defaults, reflected metadata, exception categories/messages, import source-format restrictions and all image algorithms.
- Add boundary, cross-field, import/bake parity and reflected-contract regressions.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `skybox-skylighting`: Environment owns shared bake-settings validity and preserves standalone prefilter admission.
- `editor-asset-import`: Import settings preflight delegates bake validity while retaining its format/type boundary.

## Impact

Environment's CPU API/implementation, AssetImport's existing validation/metadata, focused tests and documentation. No new module dependency, plugin behavior, transport operation, asset schema, algorithm or lifecycle change. Leave the independent change active and uncommitted.
