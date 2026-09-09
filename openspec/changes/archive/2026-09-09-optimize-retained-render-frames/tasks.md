## 1. Baseline and normal-path evidence

- [x] 1.1 Preserve first-round binaries/source state and document the current scene/material/recording costs and wait dependency graph.
- [x] 1.2 Add reusable, default-off profiling/counters for bridge, retained preparation, maintenance and frame coordination.

## 2. Incremental preparation and retained data

- [x] 2.1 Make bridge synchronization and ready-status observation incremental while preserving shared editable material updates.
- [x] 2.2 Remove maintenance triggered solely by ready view preparation; validate production, failure and no-frame retirement.
- [x] 2.3 Retain stable scene/view results with complete invalidation and conservative custom primitive fallback.
- [x] 2.4 Reuse prepared material/batch/packet data across stable frames and reduce repeated shared-scope refresh work on camera changes.
- [x] 2.5 Carry immutable draw ownership through graph/native recording and reduce repeated validation/state planning where proven safe.

## 3. Frame coordination

- [x] 3.1 Add ordered owned deferred graph preparation with valid dependency expansion and immediate-API compatibility.
- [x] 3.2 Centralize RHI preparation, BeginFrame, batched parallel recording, joins, EndFrame and cancellation under one coordinator task.
- [x] 3.3 Integrate Viewer scene/GUI/capture preparation and statistics with one ordinary Render-to-RHI boundary.

## 4. Correctness and delivery

- [x] 4.1 Add targeted invalidation, extension, ownership, current-diagnostic and failure/retry tests; run relevant suites.
- [x] 4.2 Build Debug/Release/Profile and complete style, naming, boundary and OpenSpec validation.
- [x] 4.3 Run matched draw-count/CSM static-moving benchmarks and profile the remaining work and wait critical path.
- [x] 4.4 Verify pixels, visible GUI/capture, sustained camera movement and bounded resource/retention behavior; document measured outcomes and limitations.

## 5. Independent audit corrections

- [x] 5.1 Give deferred scene/depth/GUI preparation a valid owner credential and reject work after owner closure or destruction.
- [x] 5.2 Publish current per-view draw receipts on packet reuse and make session Close retry safe after partial shutdown.
- [x] 5.3 Correct batch-plan retirement and native immutable-stream constant-page ownership/command-shell validation.
- [x] 5.4 Complete targeted multi-configuration validation, independent re-review and performance-impact evidence for the audit fixes.
