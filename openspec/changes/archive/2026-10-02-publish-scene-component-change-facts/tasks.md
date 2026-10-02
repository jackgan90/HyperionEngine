## 1. Preserve the existing domain boundary

- [x] 1.1 Read the complete scene mutation/synchronization/history/query paths and coordinate ownership; record actual targets and baseline commands.
- [x] 1.2 Add independent existing mask, revision, metadata reuse and query-update assertions where useful and run them before production migration.

## 2. Publish registered component facts

- [x] 2.1 Add owned type/instance fact values and deterministic live-instance difference computation through descriptor equality, excluding empty and opaque slots.
- [x] 2.2 Integrate facts into staged edits, add/remove/Clear/subtree operations and initial synchronization; merge unacknowledged occurrence flags before authoritative writes.
- [x] 2.3 Derive compatible built-in effects from the same differences while preserving semantic no-op identity changes, local versus inherited transforms and selective query invalidation.

## 3. Verify state and lifecycle semantics

- [x] 3.1 Test repeated custom components, add/modify/remove, rename/type replacement, no-op/empty slots and old owned change snapshots against independent identity expectations.
- [x] 3.2 Test committed add/remove/readd accumulation, acknowledgement boundaries, initial synchronization, Clear/subtrees and node slot generation reuse.
- [x] 3.3 Test prepublication callback failure and unchanged values/revisions/pending facts/query/history; exercise component facts through shared-domain Undo/Redo.
- [x] 3.4 Verify built-in model/camera/light masks, inherited transforms/default camera and metadata-only bridge/query reuse without full-scene rebuilds.

## 4. Validate and review

- [x] 4.1 Update SceneComponents documentation with occurrence, ownership, acknowledgement and opaque-component limits.
- [x] 4.2 Build and run affected scene management, editing, rendering, query, dispatch-failure and boundary tests in Debug/Release.
- [x] 4.3 Run formatting, changed-TU semantic naming, dependency boundaries, diff check and strict OpenSpec validation.
- [x] 4.4 Obtain independent review, directly verify findings, repair confirmed scoped issues and repeat affected checks; report evidence to the orchestrator.
