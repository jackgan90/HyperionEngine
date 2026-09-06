## Why

A single local-space primitive cannot represent a glTF model. Static scene data must be owned independently of renderer and vendor types.

## What Changes

- Add Scene runtime module with reflected model, geometry, material, texture and node data.
- Define stable references, roots, transforms, bounds and validation without GPU handles.
- Extend Math with quaternion/TRS, camera, inverse/normal transforms and vector operations.

## Capabilities

### New Capabilities
- `static-model-data`: A single local-space primitive cannot represent a glTF model. Static scene data must be owned independently of renderer and vendor types.

### Modified Capabilities
None. Existing configuration, triangle and task behavior is preserved.

## Impact

New Scene module, Math and reflected model tests. Depends on add-reflected-object-archives.
