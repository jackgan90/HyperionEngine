## 1. Portable transport

- [x] 1.1 Add transport interfaces, provider registry, owned byte progress and memory transport contract tests.
- [x] 1.2 Implement Windows overlapped named pipes, current-user admission, bounded IO and pending-operation cleanup.

## 2. Target protocol and frontend routing

- [x] 2.1 Add independent target discovery, portable framing, identity/version handshake and bounded connections.
- [x] 2.2 Add target-owned session host, safe update-boundary dispatch, disconnect draining and structured failures.
- [x] 2.3 Route CLI/JSONL/MCP through an asynchronous endpoint, add targets management and --attach, and preserve standalone behavior.

## 3. Shared scene authority

- [x] 3.1 Extract CPU scene document/history/save-state contracts and Renderer target bridge.
- [x] 3.2 Migrate Editor GUI editing/history/save points to the shared authority while preserving gestures and selection behavior.
- [x] 3.3 Integrate Scene Viewer through shared editing contracts with truthful history/save capabilities.
- [x] 3.4 Register reflected live-scene query, transform, undo/redo and save operations with identity/revision/busy validation.

## 4. Application attachment

- [x] 4.1 Compose default local listeners and discovery into Editor/Viewer with explicit disablement and scoped shutdown.
- [x] 4.2 Validate target isolation, unavailable providers, stale identities, disconnect and startup/shutdown failures.

## 5. Delivery validation and documentation

- [x] 5.1 Run transport/protocol/domain tests and real CLI/MCP Editor/Scene Viewer acceptance including transform, undo and save/reopen.
- [x] 5.2 Run affected GUI regressions, builds, style/naming, module boundary and OpenSpec checks; repair failures.
- [x] 5.3 Update automation, architecture, module and validation documentation with examples, extension contracts and deferred coverage.
- [x] 5.4 Verify final task/spec consistency and report the uncommitted working tree without archiving or committing.
