## Why

The renderer currently uses standard zero-to-one depth throughout and cannot select reversed-Z. Make floating-point reversed-Z the default while preserving a startup configuration fallback to standard depth across all sample applications.

## What Changes

- Add an explicit engine-owned depth convention for projections, views, pass state and depth targets.
- Default application `reversed_z` to true; load the active mode once at startup and label edits as restart-required.
- Support both conventions in Forward, Deferred, transparency, CSM and legacy frame depth, including ordinary and instanced materials.
- Enable depth testing and convention-correct clip-space depth in Triangle; update ModelViewer and SceneViewer projections.
- Preserve explicit raw material comparison operations through a separate opt-in view-relative depth state policy.
- Add CPU/GPU and application validation for both modes, configuration persistence, clipping and shadow behavior.

## Capabilities

### New Capabilities
- `depth-conventions`: Startup-selected reversed-Z and standard-Z contracts across renderer and applications.

### Modified Capabilities

None. Existing rendering capabilities retain their output contracts; depth convention support is specified by the new cross-cutting capability.

## Impact

Math, Materials, Renderer, Config, Viewer and the three scene plugins; D3D12 frame-depth optimized clear metadata; shadow shaders; experiments and tests. No new external dependencies. Runtime switching and infinite far planes are excluded.
