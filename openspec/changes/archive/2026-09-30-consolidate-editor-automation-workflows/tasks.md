## 1. Establish implementation baseline

- [x] 1.1 Read repository rules, design/spec deltas and relevant current code; record exact build/test entry points and verify review evidence before edits.

## 2. Shared scene validation and hierarchy filtering

- [x] 2.1 Introduce cheap document/revision/handle validation and migrate authoring/component callers, avoiding full-scene queries used only for validation.
- [x] 2.2 Validate common batch state once; share ordered selected-root filtering with early ancestor coverage termination across document and structural operations.
- [x] 2.3 Add deterministic target-call/traversal regression coverage for single edits, batches, deep selected hierarchies, order and invalid-input atomicity; run affected SceneEditing tests.

## 3. Shared asynchronous asset edit workflow

- [x] 3.1 Implement focused AssetEditing async workflow ownership for texture encoding and reference edits with snapshots, busy admission, generation/identity checks, shared commit/history and controlled errors.
- [x] 3.2 Migrate GUI asset workspace and automation standalone/attached documents to the common workflow; preserve session, close, root-change and quiesce/drain lifetimes.
- [x] 3.3 Cover success parity, stale completion, preparation/reference failure, busy cleanup, close and shutdown with meaningful tests; run affected asset and automation tests.

## 4. Input contracts and shared shortcut routing

- [x] 4.1 Add named engine input buttons/modifiers and migrate relevant adapter/Editor literals while preserving numeric values and SDK boundaries.
- [x] 4.2 Introduce current-frame shortcut interaction snapshot and explicit per-command admission; migrate delete, selection, clipboard, framing and history, retaining inspector transaction semantics.
- [x] 4.3 Exercise real routing for eligible scene focus, unrelated panels, text, ordinary popup/modal, gestures, focus loss and same-batch ownership transitions; verify no unintended mutation/history.

## 5. Import semantic metadata and named limits

- [x] 5.1 Resolve GUI import type/settings/source filters through stable capability IDs instead of hard-coded type indices; verify reordered capabilities and preserved supported formats.
- [x] 5.2 Add reflected typed texture dimensions and byte size; migrate GUI away from Details parsing and use an explicit common preview page limit; verify schema and inspection parity.
- [x] 5.3 Name automation peer/session/handshake/dispatch/connection budgets with shared constants for actual shared invariants, preserving defaults; remove duplicate angle literals through appropriate unit helpers.

## 6. Editor ownership and acceptance isolation

- [x] 6.1 Extract acceptance state/driver and semantic phase/input-window routing; remove production numeric step-range interpretation and gate acceptance implementation with BUILD_TESTING.
- [x] 6.2 Extract focused viewport and document-transition owners with narrow dependencies and move relevant behavior, retaining plugin lifecycle/service registration and renderer lifetime rules.
- [x] 6.3 Split mixed-responsibility AdvanceFrame, discard-dialog, Render and host startup functions at logical boundaries; check function length per CodingStyle without mechanically splitting declarative tables.
- [x] 6.4 Build Editor with tests enabled and disabled; validate retained acceptance flags or controlled unavailable diagnostics, optional provider absence and quiesce/shutdown paths.

## 7. Automation adapters and stale material

- [x] 7.1 Consolidate private typed scene registration/error conversion helpers; verify operation discovery, describe schemas/metadata and representative invocation remain compatible.
- [x] 7.2 Remove verified-unused private node-history/handle-hash/remap aliases and redirect meaningful tests to live shared contracts, preserving serialized compatibility.
- [x] 7.3 Correct stale scene settings, asset unsupported-type, light priority and shadow descriptions in API metadata and active docs; update architecture/capability docs for actual ownership and additive DTOs.

## 8. Integration validation and independent review

- [x] 8.1 Run changed-file format/style checks, module boundary checks and strict OpenSpec validation; resolve findings through the implementation agent.
- [x] 8.2 Build required Debug and Release targets and run appropriate SceneEditing, AssetEditing, AssetImport, Automation and Editor tests plus affected input/lifecycle acceptance checks. Record commands, outcomes and any environment limitations.
- [x] 8.3 Orchestrator reviews the final diff against all ten problem groups and spec scenarios; implementation agent addresses review findings and reruns checks affected by repairs.
- [x] 8.4 Record concise implementation/validation evidence in this active change, verify final worktree scope, and deliver without OpenSpec archive, git commit or push.
