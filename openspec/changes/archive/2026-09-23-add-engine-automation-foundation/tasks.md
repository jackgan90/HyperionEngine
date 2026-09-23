## 1. Reflected wire contracts

- [x] 1.1 Add bounded JSON parsing/encoding and strict reflected natural-JSON projection without changing archive formats.
- [x] 1.2 Generate schemas with required/default fields, enum values, fixed arrays, descriptions and exact wide integers; cover rejection and extension cases.

## 2. Automation runtime

- [x] 2.1 Add typed operation registration, deterministic bounded search, lazy descriptions, availability and catalog sealing.
- [x] 2.2 Add Main-owned invocation, structured errors, bounded asynchronous jobs and shutdown admission rules.
- [x] 2.3 Add a shared bootstrap endpoint and a pinned MCP stdio protocol adapter with lifecycle/error tests.

## 3. Shared domain services

- [x] 3.1 Extract asset document/history/save and texture encoding helpers to CPU-only AssetEditing; migrate Editor consumers.
- [x] 3.2 Register asset document open/info/rename/undo/redo/save/close and texture encoding through the shared implementation with revision checks.
- [x] 3.3 Validate identical domain behavior, save races/conflicts, invalid/stale handles, busy edits and dirty close.

## 4. Application and transport

- [x] 4.1 Compose automation, optional asset provider and stream transport through scoped plugins; preserve minimal Application and optional branches.
- [x] 4.2 Provide one-shot CLI, persistent JSON-lines and MCP modes with clean stdout, bounded framing, EOF and pending-work cleanup.
- [x] 4.3 Exercise end-to-end CLI/MCP discovery, edits, jobs, save/reopen and disabled/startup-failure/shutdown paths.

## 5. Extension contract and delivery validation

- [x] 5.1 Document architecture, usage, extension recipe, protocol versions, limits and current/deferred capability coverage; update development rules and indexes.
- [x] 5.2 Run appropriate Debug/Release builds, new contract tests, affected asset/editor/plugin regressions, style/naming/boundary checks and strict OpenSpec validation.
- [x] 5.3 Inspect final diff and record verification and remaining adaptation scope without committing or archiving.
