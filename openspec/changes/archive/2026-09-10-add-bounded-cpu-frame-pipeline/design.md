## Context

Main currently waits for a Render task that itself waits for RHI 0. RHI 0 prepares the graph, begins one swapchain frame, joins peer recording tasks and submits/presents. D3D12 permits one active CPU recording frame and independently retires GPU resources by fences. Existing immutable material/view snapshots are useful, but Viewer callbacks capture stack references and Session/Pipeline expose only their latest preparation statistics.

## Goals / Non-Goals

Goals: independent bounded Main/Render and Render/RHI CPU overlap, deterministic ordered frames, owned cross-domain inputs/results, safe lifecycle/error handling and observable limits.

Non-goals: GPU scheduling changes, zero RHI threads, out-of-order frame completion, concurrent RHI recording of multiple frames, runtime lead reconfiguration, specialized lock-free data queues, OpenSpec archive and Git commit.

## Decisions

1. Add a Renderer-owned frame pipeline over the existing task system. Main submits an owned Render callable which returns an owned RHI callable; the pipeline routes these to Render and RHI 0. RHI 0 retains same-frame peer recording and ordered submission. Each engine tick receives a monotonically increasing ID, including skipped drawing ticks.
2. After submitting frame N, Main cannot begin N+1 until Render has completed N-a; Render cannot begin N+1 until RHI has completed N-b. Clamp startup thresholds at zero. A zero limit waits for the just-dispatched stage; a=0 alone need not wait for RHI when b>0. Queued frame handles, rather than executor task counts, enforce this contract. RHI completion includes all peer recordings and submission callbacks, not GPU completion. Empty frames enqueue an ordered no-op RHI stage.
3. Lead settings are startup-only nonnegative integers with a documented finite capacity limit, default one. Existing configurations remain valid. Immutable service pointers may be captured by value only where explicit drain guarantees their lifetime; mutable input values and per-frame results have separate owned storage. A specialized data queue is unnecessary for this iteration.
4. Expose a per-view-family preparation token and per-forward-frame result snapshot. Deferred RHI preparation publishes completion before readers access statistics. Existing synchronous Complete/Statistics compatibility interfaces remain available; queued frames capture their own token before another build overwrites the current one.
5. Add an RHI-domain graph execution entry point for pipeline jobs and retain synchronous ExecuteGraph wrappers. Viewer supplies shadow/settings/view/GUI/material snapshots and callbacks owned by the frame. Main alone updates display metrics and writes files after ticket completion. Benchmark rows correlate tick IDs with the same frame's results, preserving existing columns and recording async latency separately.
6. Stop admission on errors, retain and join every admitted job, and observe exceptions even when dependencies fail. Never implement mandatory cleanup as a success-only dependent body. Drain the pipeline before plugin/session/device release and before final output verification. Main waits retain task pumping; normal polling continues between bounded ticks. Resize remains ordered on RHI with native fence waits. Explicit capture requests are attached to the target frame on RHI 0 so earlier queued frames cannot steal them.

## Risks / Trade-offs

- Increased live frame memory and input latency -> finite limits, bounded ticket/result retention, one-frame defaults and visible stage counters.
- Render/RHI cache overlap -> audit shared mutable state, keep cache ownership on its existing domain or use existing locks, and ensure old snapshots retain immutable data.
- Delayed completion can overwrite diagnostics -> results use the original frame token and are consumed in order on Main.
- Capture/resize/shutdown interacting with outstanding work -> ordered control operations and explicit CPU drain; retain existing GPU fence safety.
- Wait cycles -> pipeline only waits downstream; RHI work must not synchronously wait on Render, and all peer recordings are joined before coordinator completion.

## Migration Plan

Implement reusable scheduling and per-frame tokens, migrate Viewer, then add configuration and acceptance tests. Both limits default to one. Existing configurations without these keys use the new defaults; explicit zero limits provide the synchronous execution mode using the same code path. No serialized key removal or GPU resource protocol migration is required.

## Open Questions

None requiring user input. Implementation details and verified evidence will be recorded alongside this change.
