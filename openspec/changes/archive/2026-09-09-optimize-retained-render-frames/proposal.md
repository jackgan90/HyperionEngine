## Why

The first CPU submission optimization still rebuilds stable scene/view work and repeats object-level checks on every frame. Current traces also show separate Render-to-RHI round trips for preparation, BeginFrame, recording joins, and submission, while unconditional resource maintenance adds background work to ready scenes.

## What Changes

- Preserve the first optimization's binaries and measurements and trace the complete Main update, visibility, material, packet preparation, recording, and synchronization path.
- Retain stable scene/view preparation and immutable draw data with explicit scene, resource, view, material-scope, and extension invalidation. Camera changes update dependent work; custom collection/strategies retain conservative behavior.
- Make scene bridge synchronization and resource maintenance respond to changes instead of scanning and scheduling unchanged ready state on each view.
- Consolidate frame preparation and submission under one RHI coordinator task. Batch recording by executor and keep exception cleanup and GPU fence ownership intact.
- Reduce residual repeated material/native draw work using immutable data and validated compatibility, with counters and A/B evidence.

## Capabilities

### New Capabilities

- `retained-render-frames`: dependency-aware reuse of stable CPU rendering work and measurable invalidation.

### Modified Capabilities

- `scene-management`: incremental synchronization of logical changes and editable material revisions.
- `render-graph-plugins`: deferred owned RHI preparation and a centralized frame execution boundary.
- `shared-render-resources`: completion-driven maintenance without steady ready-view polling.

## Impact

Renderer scene/session/graph/material caches, the scene bridge, Viewer and Debug UI integration, D3D12 recording, and targeted tests/profiling are affected. This builds on the unarchived `optimize-renderer-cpu-submission` change; its results remain the A/B baseline. No external dependency, scene quality, shader precision, frame latency policy, validation setting, or public target/executable name changes are intended.
