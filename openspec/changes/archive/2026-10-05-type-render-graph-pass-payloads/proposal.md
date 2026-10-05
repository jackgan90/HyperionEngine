## Why

RenderGraph stores graphics and compute declarations in FGraphicsPass with a boolean and unrelated fields, making invalid combinations representable. Single/multiple color inputs also persist as duplicate internal attachment storage.

## What Changes

- Store accepted graph passes as common scheduling facts plus a private typed graphics/compute variant.
- Normalize legacy Color into the authoritative color vector at admission and reject simultaneous Color/Colors before publishing the pass.
- Remove compute-only fields from graphics declarations and keep existing public graphics/compute entry points.
- Preserve validation stages, content hazards, resource resolution, callbacks, copy/consume execution and native submission behavior.

## Capabilities

### New Capabilities

- `typed-render-graph-pass-payloads`: Explicit accepted pass payload ownership and normalized attachment admission with staged validation guarantees.

### Modified Capabilities

None. Graph execution capabilities and scheduling behavior remain unchanged.

## Impact

Renderer graph declaration/storage, topology/resource/buffer validators, preparation and regression tests. No new queue scheduling, transient allocator or rendering feature is introduced.
