## Context

The standalone automation host owns asset drafts and one Main-owned session. Editor owns scene history, save points, selection and interaction state privately; Scene Viewer edits its live SceneInstance without undo history. TaskSystem waits can pump Main tasks, so merely dispatching a received request to Main does not provide an editing-safe boundary. Platform currently links SDL and is unsuitable as a mandatory headless communication dependency.

## Goals / Non-Goals

**Goals:** maintainable transport substitution; target-owned schemas and execution; default current-user local attachment; GUI/agent use of the same scene document instance; bounded asynchronous IO and safe shutdown; explicit identity, revision and completion contracts.

**Non-Goals:** production TCP/TLS, Unix/macOS providers, device discovery, automatic reconnect/replay, resumable sessions, bulk transfer, all engine APIs, attached asset drafts, network exposure, hot code reload, archive or commit.

## Decisions

### Transport abstraction and composition

Add Runtime/Transport with ITransportConnection, ITransportListener, ITransportProvider and FTransportRegistry. Connections expose asynchronous/polled, full-duplex ordered byte progress with owned buffers, explicit partial progress/EOF/errors and idempotent close. Registry keys are transport schemes rather than an OS enum. Implement Windows named pipes privately with overlapped IO, explicit current-user access control and remote rejection. IO state outlives pending native requests. Native code never executes domain callbacks. An in-memory provider exercises identical contracts, partial IO, backpressure and disconnects. Register compiled providers explicitly; do not add global service locators or runtime plugin loading.

Keep target discovery independent of transport. ITargetDiscovery lists advisory descriptors; explicit structured addresses also connect without discovery. Local runtime records contain an instance ID and address, not content roots or engine configuration. PID and paths are optional metadata. Handshake validates the expected boot-specific instance identity; stale records cannot redirect a call. Unsupported schemes return a structured error.

### Shared framing and automation protocol

Use a bounded length-prefixed UTF-8 JSON protocol over byte streams, with explicit byte order, request correlation and a versioned hello exchange. Separate communication version, operation contract versions and build identity. Negotiate limits conservatively under fixed local ceilings. Reuse reflected wire values and existing domain response limits. Protocol roles are independent of connect/accept direction; only CLI-initiated attachment is implemented now. Future transports reuse framing, routing, schemas and invocation. Authentication facts originate in verified transport metadata, not self-reported hello fields; the first policy admits current-user local peers only. Network authentication remains unimplemented and cannot silently fall back to local trust.

InstanceId identifies a running application, ConnectionId a frontend connection and SessionId a target-owned job context. Shared application documents outlive connections. Every connection gets a bounded target session. Disconnect stops admission; already admitted work is drained while providers live. This version does not resume sessions or automatically retry mutations. Timeouts after transmission report unknown outcome rather than rollback.

### Frontends and lifecycle

Retain standalone CLI/JSONL/MCP behavior. Add targets.list/connect/disconnect and optional connection routing on existing bootstrap methods; --attach establishes a default target. Explicit routing never falls back to standalone or another instance. Attached search/describe/type queries are answered by the target. Frontends depend on a generic asynchronous request interface rather than a concrete local session; one-shot jobs are polled through that same interface.

Application automation consists of catalog, operation providers, session management and local listener plugins. Normal Editor/Viewer composition requests the listener by default; --disable-plugin automation-local wins. Failure/absence affects only automation. Listener/session consumers start after domain providers and quiesce before their destruction. Runtime/Application retains Tasks, Main pumping, time and exit only. Incoming requests are consumed with per-update budgets at plugin Update boundaries, not in arbitrary reentrant Main task callbacks. Non-drawable windows still progress control requests.

### Shared scene authority

Add Runtime/SceneEditing for CPU scene document history, selection/history identity, save-point tracking and typed operation values. A narrow target interface bridges Renderer SceneInstance validation, resource rebinding and snapshots without making SceneEditing depend on Renderer/RHI. Migrate the existing Editor history as one shared authority, including structural and settings edits and selection restoration; do not add a parallel automation undo stack. GUI interactions retain their UI behavior and use the same service. Service-level interaction/busy gates prevent remote writes during active Inspector/Gizmo/placement/modal transactions.

Expose current scene info, bounded node listing and detail, atomic transform edits with expected document/scene identity and revision, undo/redo where supported, and explicit save. Scene Viewer shares the editing entry but truthfully reports no history and preserves its save semantics. A successful mutation means committed on Main, not disk persistence or GPU presentation. Document epoch and generation-safe handles reject use after load/close/root replacement. Absolute transform updates preserve unrelated properties and do not implicitly select objects.

Document dirty state follows authored transactions, not raw scene revisions: asynchronous resource preparation also advances revisions. All Scene Viewer UI authoring enters the document service. Resource-preserving duplicate and remove-while-keeping-children have no-history adapters; history hosts reject these entry points until an undo adapter exists. Their agent adapters remain deferred.

Do not install the standalone asset-draft provider into GUI hosts. Attached asset editing and unsafe root mutation adapters remain explicitly unavailable until they share the host workspace and all content participants. Root state can be inspected without introducing ContentMounts files.

### Paths and future remote targets

Addresses are provider-interpreted UTF-8 values, not public native handles. Domain paths resolve on the target; --json-file is frontend-local. Future artifact transfer is a separate capability. macOS/Linux add stream transport, peer identity and discovery adapters; mobile access additionally requires network authentication/transport and optional discovery, while existing sessions and operations remain reusable.

## Risks / Trade-offs

- Editor history extraction may regress gestures/selection/save points → migrate authority without replacing GUI behavior and run existing interaction tests plus attached undo/save acceptance.
- IO may outlive captured state → cancellation/join before destruction, bounded queues, native pending-operation cleanup and failure/disable tests.
- Main waits can reenter → receive queues consumed only at plugin update boundaries, with domain busy/revision validation at execution time.
- A reply can be lost after mutation → no automatic replay; report outcome uncertainty and allow state re-query.
- Other platform support is architectural, not validated production support → use portable public contracts and fault-injecting memory transport tests; document unimplemented providers.
- Current-user local default permits that user to automate the app → explicit disablement; no network listener is introduced.

## Migration Plan

Implement transport and protocol first, preserve standalone tests, then introduce the shared scene authority and attach hosts. Existing applications must be rebuilt/restarted once to acquire listeners. Existing --mcp configuration remains usable through targets.connect. Disable automation-local to restore the prior application exposure while keeping GUI behavior. Document integration requirements and explicit deferred coverage. Leave the completed change active and the working tree uncommitted.

## Open Questions

None blocking implementation. Concrete limits and public names will be recorded with tests and usage documentation; incompatible scope changes require revisiting this design.
