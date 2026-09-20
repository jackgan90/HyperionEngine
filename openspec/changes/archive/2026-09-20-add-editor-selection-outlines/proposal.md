## Why

Editor selection currently updates the Outliner, Details and gizmo without identifying the selected model silhouette. An orange outline visible through occluders will make selection readable; selectable union and per-object modes let users compare clarity against cost before multi-selection UI exists.

## What Changes

- Add reusable Renderer silhouette-mask, outer-edge extraction and post-tonemap outline composition through IRenderFeature.
- Support immutable multi-object requests with generation-safe primitive groups and matching scene publications.
- Default to union silhouettes; provide an immediate viewport option for independent per-object outlines.
- Connect existing Outliner and viewport selection without changing document state or implementing multi-selection gestures.
- Preserve standard PBR alpha masking and effective instance/section inputs; expose an explicit mask pass contract for custom materials.
- Add GPU image tests and an editor comparison exercise with multiple targets, lifecycle coverage and documentation.

## Capabilities

### New Capabilities
- `selection-outlines`: reusable depth-independent silhouette outlines, overlap policies, publication ownership and editor activation.

### Modified Capabilities

None. Existing selection and input contracts remain unchanged.

## Impact

Runtime/Renderer scene bridge, target collection and render features; Editor viewport configuration and frame submission; engine shaders and targeted tests. Scene remains CPU-only. No new plugin dependency, application lifecycle or asset format is required. No commit or push is authorized.
