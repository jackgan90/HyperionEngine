## Context

At baseline `afdb45e`, `RenderBenchmark.cpp` classifies shadow, Forward, Deferred, transparent, output, sky, HZB, contact and local-light work with string prefixes. Graphics graph passes expand into command batches with `/N` suffixes; compute passes keep their name. D3D12 retains immutable `FPassCommands` until fence completion and publishes `FGpuPassTiming` with only name and duration. `ViewDiagnosticsTests.cpp` already has a checked-in 138-column CSV oracle. GPU capture separately enforces submission identity and capture epochs.

## Goals / Non-Goals

**Goals:** Explicit Renderer category and category-local instance, exact transport through split/deferred/copy/consuming graph paths, name-independent aggregation, unchanged diagnostic wire/CSV output for built-in workloads.

**Non-Goals:** New profiler, timers, logging, shader/render behavior, plugin composition, GUI/automation controls, new native backends, performance claims, archive or commit. Existing capture epochs, fence collection, capacities and submission matching remain authoritative.

## Decisions

1. Renderer owns `ERenderPassTimingCategory` and `FRenderPassTiming` (category plus a 32-bit instance). Categories follow existing benchmark buckets; all Deferred lighting variants intentionally share Lighting. Shadow instances are cascade indices; HZB instances are mip indices. Unclassified is the default. These are runtime metadata, not reflected/persisted API fields.
2. RHI owns a small `FGpuTimingTag` value with opaque Domain and Value words. Renderer alone encodes its domain, category and instance and decodes recognized values. A foreign domain or unknown category is unclassified for Renderer aggregation; invalid typed category construction is rejected by the encoder. This avoids RHI dependencies on Renderer, name parsing, borrowed pointers, global registries and per-pass allocation. A bare category enum in RHI was rejected because it would move pipeline semantics into the backend contract.
3. Append timing fields to existing public aggregates to preserve positional initializers. Carry Renderer metadata through `FRenderPassTargets`, graphics/compute graph declarations and `FComputePassDesc`; encode when compiling commands. Every expanded graphics batch inherits the same timing identity. D3D12 reads the tag from its retained immutable commands when collecting the completed result, avoiding another independently maintained native field.
4. Built-in owners assign categories explicitly: CSM, both Forward pipelines, Deferred base/compatibility/lighting/local lights, transparency, tonemap, sky, HZB and contact mask. Display-only/debug/GUI and other unclassified work contributes only to the total. Names remain unchanged for HUD, capture labels and existing graph name validation.
5. One Renderer aggregation function returns named duration fields and the existing four cascade slots. CSV writing projects those fields explicitly, preserving order, spelling, precision and lighting membership. Unknown tags and out-of-range cascade indices cannot index storage; a recognized shadow with a larger instance still contributes to shadow and total time.
6. Timing metadata participates in the existing pass-target value equality, so retained snapshots observe updated metadata. It does not become a graphics PSO or native draw-plan key: those caches own rendering work, while per-submission commands own timing attribution.

## Risks / Trade-offs

- Missed producer → test real Forward/Deferred/shadow/HZB/contact/local/sky results and preserve the existing CSV oracle.
- Lost tag on split/copy/deferred/native batching → graph and native tests use distinct nonzero tags, renamed labels and multiple batches/passes.
- Custom passes formerly using reserved prefixes → document explicit category opt-in; do not retain a string fallback that reintroduces ambiguous identity.
- Delayed results or disabled timing → keep lifecycle code unchanged and rerun capture/cancellation/epoch regressions. No additional wait, fence, log or GPU work is introduced.
- Existing oversized native swapchain code → avoid editing it by using its already-retained immutable command metadata at collection; do not expand this change into unrelated lifecycle refactoring.

## Migration Plan

Build all affected consumers after adding defaulted aggregate fields, assign built-in categories and replace aggregation together. Run Debug and Release targeted regressions plus style/boundary/spec checks. Rollback is a source-only revert; no disk/schema migration is necessary.

## Open Questions

None for this scoped change.
