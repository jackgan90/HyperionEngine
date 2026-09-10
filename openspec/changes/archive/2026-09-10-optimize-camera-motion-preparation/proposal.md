## Why

The current Debug SceneViewer drops from 6.35 ms at rest to 20.61 ms during even a small camera orbit, while the main view still submits six draws. Changing a view invalidates the whole prepared snapshot and makes collection, material evaluation, batching and RHI preparation independently revalidate hundreds of otherwise unchanged items across five views.

## What Changes

- Retain validated local preparation independently from changing view/shared inputs, and refresh compatible material dependency groups once per view.
- Reuse the resulting proofs across visibility collection, batch membership and draw preparation instead of rediscovering stable state at each layer.
- Retain bounded preparation for items temporarily outside a view so camera movement does not repeatedly destroy and recreate their local state.
- Add permanent gated diagnostic scopes and operation counts, then measure static, small-orbit and changing-visibility workloads using frozen baseline binaries.
- Preserve arbitrary material dependencies, custom strategies, ordering, failure publication, old-frame ownership, native validation and GPU retirement.

## Capabilities

### New Capabilities

- `camera-motion-preparation`: Dependency-granular retained CPU preparation under camera and visibility changes, with explicit validity boundaries and reproducible Debug measurements.

### Modified Capabilities

None. Existing rendering, material, batching and resource-lifetime behavior remains required.

## Impact

Renderer collection, material preparation, batching, pass preparation and their private caches; Renderer tests and existing Viewer performance diagnostics. Any new shared state remains engine-owned. No new third-party dependency, native backend selection, shader ABI, quality reduction, Debug compiler change or profiling-default change is intended.
