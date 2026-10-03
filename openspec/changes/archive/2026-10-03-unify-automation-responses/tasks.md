## 1. Compatibility baseline

- [x] 1.1 Add fixed original outcome/job snapshots and run them plus existing automation contract/connection/transport tests before production edits.

## 2. Shared response contract

- [x] 2.1 Add response builders, typed status/views and bounded structural validation with focused tests.
- [x] 2.2 Migrate Jobs and connection producers, preserving encoding budgets and job lifecycle.
- [x] 2.3 Migrate stdio, MCP and connection readers, preserving valid frontend behavior and controlled malformed-response handling.

## 3. Verification and delivery

- [x] 3.1 Test malformed handshake/routed replies and immediate/deferred MCP results; verify snapshots and no fallback/replay.
- [x] 3.2 Build Debug/Release and pass affected CLI/MCP, attachment, discovery, availability and lifecycle regressions.
- [x] 3.3 Update documentation/evidence; pass formatting, semantic naming, dependency, diff and strict OpenSpec validation; leave active and uncommitted.

## 4. Quality-audit follow-up

- [x] 4.1 Classify malformed completed handshake payloads as protocol_error, preserve peer/policy failures, pass regression and attached frontend checks, and obtain targeted independent re-review.
