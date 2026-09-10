## 1. Frame execution and ownership

- [x] 1.1 Implement Renderer-owned ordered frame tickets, two lead limits, progress, failure observation and drain.
- [x] 1.2 Add owned RHI graph execution and preserve coordinator peer joins and synchronous wrappers.
- [x] 1.3 Add frame-specific view-family and forward-pipeline result tokens; audit Render/RHI mutable cache boundaries.

## 2. Viewer integration

- [x] 2.1 Add validated startup configuration/CLI for both limits, using defaults of one and mandatory RHI executors.
- [x] 2.2 Migrate Viewer to owned input/result packets, ordered skipped ticks, Main-owned diagnostics and correlated benchmark rows.
- [x] 2.3 Integrate capture targeting, screenshot completion, resize and drain before verification/teardown on normal and exceptional exits.

## 3. Validation and documentation

- [x] 3.1 Add deterministic limit/overlap/order/failure/drain tests, including a slow peer and unused RHI executors.
- [x] 3.2 Add configuration, delayed preparation ownership and native asynchronous Viewer acceptance coverage.
- [x] 3.3 Run appropriate Debug/Release builds and tests, style/naming/boundary and OpenSpec checks; document verified behavior and remaining limits without committing.
