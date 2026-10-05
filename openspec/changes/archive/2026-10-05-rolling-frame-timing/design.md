## Context

ApplicationHost computes the interval between successive update starts with a steady clock. Editor stores that interval in `FrameIntervalMilliseconds`, and the HUD refreshes one diagnostics snapshot every 100 ms. The interval includes scheduling and waits; it is neither exclusive CPU execution nor GPU duration. Investigation found real fence waits as well as the misleading single-frame presentation.

## Goals / Non-Goals

**Goals:** Collect every Editor interval regardless of HUD visibility or profiling-provider availability; present sustained throughput and preserve evidence of individual long frames; share typed data between HUD and automation; bound storage and avoid sorting/allocation in the collection path.

**Non-Goals:** Change VSync, presentation, task scheduling, frame clock precision, GPU timing, or separate asset preview behavior. This change does not claim to eliminate real spikes.

## Decisions

1. Renderer owns `FFrameTimingWindow` and its reflected `FFrameIntervalStatistics` value. Editor owns one accumulator on Main, records the existing interval in `AdvanceFrame`, and attaches the snapshot to `FRenderDiagnostics`. The first synthetic ApplicationHost interval is excluded from the window; the original raw field remains unchanged. Automation reuses the existing diagnostics operation with no transport logic.
2. Retain the shortest suffix of complete frame intervals covering at least 1000 ms, or all available samples during warmup. Whole-frame coverage avoids splitting a spike across the boundary. Expose both the target and actual covered duration. A fixed 4096-element ring bounds collection storage; capacity-limited snapshots explicitly say so when the window cannot cover one second.
3. Average milliseconds is total covered time divided by sample count. FPS is 1000 times sample count divided by total time, never the average of instantaneous FPS. P95 uses the nearest-rank convention. Last and maximum use the same retained samples. Count intervals strictly greater than 1000/60 ms and expose that threshold. This is an explicit 60 FPS budget marker, not a claim about the monitor refresh rate or a comprehensive spike detector.
4. Snapshot calculation copies retained samples and selects P95 only on a diagnostics query (including the existing 100 ms HUD refresh). Per-frame recording uses the existing delta, a ring insertion and amortized eviction, with no new clock, logging or allocation. Invalid nonpositive/nonfinite samples are ignored. Long intervals, including stalls longer than the target window, remain visible.
5. Add `frameIntervalStatistics` to the reflected diagnostics result and discovery schema. Existing operation/type IDs and versions, revisions and the raw `frameIntervalMilliseconds` meaning remain compatible. No new mutable operation or persisted setting is needed.

## Risks / Trade-offs

- [Averaging delays visible changes] → Label average and window explicitly; keep Last, P95 and Max visible.
- [A percentile can hide a rare spike] → Display Max and long-frame count beside P95.
- [Very high FPS exceeds ring capacity] → Expose actual coverage and `capacityLimited`; display the actual window rather than claim one second.
- [A complete boundary-crossing frame exceeds target coverage] → Display actual covered seconds and document whole-frame retention, including a single multi-second stall.
- [Additional diagnostics work perturbs measurements] → Bound the sample count, collect without allocation, and calculate summary statistics only when queried; validate with profiling collection disabled as in ordinary Editor use.

## Migration Plan

No saved-data migration. Build RelWithDebInfo, run deterministic statistics/reflection tests and existing render-control acceptance, then verify the Sponza HUD and `render.statistics`. Leave all changes uncommitted for user acceptance.
