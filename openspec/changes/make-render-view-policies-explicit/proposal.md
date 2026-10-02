## Why

Renderer currently uses the material pass label `ShadowDepth` to decide transient participation and statistics classification, while draw/pick/legacy route lists repeat related pass names independently. This couples open material usage names to unrelated behavior and makes new views or routes easy to classify inconsistently.

## What Changes

- Add small owned view policy values for transient additions, transient replacements and statistics participation, independent of material Usage.
- Carry applicable policy through collection/preparation cache comparisons and immutable statistics snapshots.
- Migrate built-in shadow and main view producers to explicit policies while retaining their existing behavior.
- Describe built-in scene routes once for stable usage, pipeline eligibility, pick eligibility and legacy exclusion; preserve explicit target/resource construction and pass order.
- Verify custom usages, policy-only cache changes, transient replacement behavior, immutable old snapshots and independent draw/pick matrices.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `render-primitives`: Owned view behavior policy remains independent of open material usage labels and is respected by retained collection/preparation.
- `deferred-render-pipeline`: Built-in draw/pick/legacy route decisions derive from a shared Renderer-owned description without changing stage/resource ordering.

## Impact

Renderer view/session/transient/statistics and scene-route construction, related CPU/GPU tests and rendering documentation. Existing material Usage strings, persisted assets, diagnostic wire records and plugin extension topology remain compatible. M03A policy and M03B routes are sequential acceptance checkpoints within one change.
