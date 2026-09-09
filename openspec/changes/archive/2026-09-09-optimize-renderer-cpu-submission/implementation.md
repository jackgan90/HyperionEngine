# Implementation evidence

All 18 development tasks are complete. The implementation, reproducible commands, measured distributions, operation counts, retained limitations and evidence links are documented in [RendererCpuPerformance.md](../../../docs/RendererCpuPerformance.md).

- Full Debug, Release and Profile builds pass. Debug 48/48 and Release 43/43 CTest pass; Profile's 42 regular tests pass and its real Tracy acceptance passes after correcting an obsolete single-view counter expectation for four-cascade rendering. Final focused tests pass after the last rebuild.
- Formatting, semantic naming (179 translation units), dependency boundaries (280 sources / 25 modules), whitespace and strict OpenSpec validation pass.
- The final alternating Debug/Release engine benchmark contains 48,000 sampled frames. Independent prepared-packet recording covers 0/1/100/300/600/1200 draws with two reversed-order trials per configuration. Source-item coverage, CPU/GPU frame identity and enabled D3D12 validation are checked.
- Four final static/moving Forward/CSM image pairs are pixel-identical. A first one-pixel mismatch also reproduced between two baseline runs; its original evidence and independent ordinary-path comparison are retained. Visible GUI output was inspected separately.
- Four sustained runs contain 16,000 sampled frames, including 100x the default camera step. Tail PSO, descriptor and native list creation counts are constant; GPU allocation variation remains bounded.

The Debug 600-draw static P95 target of 16.67 ms and the Release moving-scene CSM added CPU preparation target of 1 ms remain unmet: final measurements are 27.489 ms and 1.249 ms respectively. These were engineering targets, not promised completion thresholds; the proposal explicitly requires reporting the measured gap. Debug optimization, native validation and shadow workload remain unchanged.

Local raw artifacts are under `out/cpu-submission-20260909`, excluded from source control. Final binary identities and per-run commands are in the benchmark summaries; intermediate stage binaries and initial baseline evidence are preserved. This change is ready for a separate archive/commit action.
