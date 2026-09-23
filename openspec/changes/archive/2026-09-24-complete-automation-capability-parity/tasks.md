## 1. Coverage and contracts

- [x] 1.1 Inventory existing Editor, Viewer and AssetTool task families with owning services, adapters and parity checks.
- [x] 1.2 Define bounded reflected editing and host-service contracts without changing transport/session architecture.

## 2. Scene authoring

- [x] 2.1 Expose shared ordered selection, node metadata editing, creation, subtree deletion and reparenting.
- [x] 2.2 Expose component discovery, read/write/add/remove and scene settings through reflected, validated document transactions.
- [x] 2.3 Expose existing placement and scene model/material reference workflows through shared preparation and commit services.
- [x] 2.4 Verify atomic rejection, stale handles/revisions, GUI-visible history and saved reload for the new scene operations.

## 3. Content and document orchestration

- [x] 3.1 Extract and reuse UI-independent scene open/close and dirty-decision orchestration.
- [x] 3.2 Enable attached root set/clear using host participants, busy checks and preference persistence.
- [x] 3.3 Expose bounded content discovery and document readiness/status.
- [x] 3.4 Validate dirty/busy rejection, successful transitions, invalidation and optional provider absence.

## 4. Asset workspace and editing

- [x] 4.1 Publish a typed workspace service backed by the actual Editor documents and reuse it in asset automation registration.
- [x] 4.2 Expose workspace listing/activation/open/close/save/history with document generation and interaction admission.
- [x] 4.3 Share and expose existing model, material, texture and sky property/reference editing, including resource preparation and preview invalidation.
- [x] 4.4 Validate standalone/live parity, reference failures, stale completions, conflict/read-only rejection and shutdown.

## 5. Views, rendering and results

- [x] 5.1 Expose existing Editor/Viewer view camera, frame-scene, preview-camera and scene-camera authoring controls through shared methods.
- [x] 5.2 Expose existing render/preview settings, diagnostics and persistent preferences with truthful host availability.
- [x] 5.3 Adapt existing capture/readback/status workflows to jobs and bounded target-local artifact metadata.
- [x] 5.4 Verify temporary versus persistent state, readiness/completion, absent providers and capture lifecycle.

## 6. Import and publication

- [x] 6.1 Register existing supported import/publication workflows using domain services and owned jobs.
- [x] 6.2 Validate import, publication conflict/cancellation and reopening the resulting native assets.

## 7. Acceptance and documentation

- [x] 7.1 Exercise discovery/describe/call/jobs through real CLI and MCP attached to Editor and Viewer for complete authoring workflows.
- [x] 7.2 Run affected builds, style, module boundary and domain/GUI regression checks; resolve failures.
- [x] 7.3 Update capability inventory and developer/user documentation with verified operations, completion semantics and remaining explicit non-goals.
- [x] 7.4 Validate OpenSpec artifacts and leave all changes uncommitted.

## 8. Audit corrections

- [x] 8.1 Reject unprepared model components through shared GUI/agent admission and restore Viewer deletion selection.
- [x] 8.2 Expose failed/loading workspace entries and safe close/retry; support atomic heterogeneous component batches.
- [x] 8.3 Adapt ModelViewer view/light controls, preserve terminal diagnostic errors and allow drawable failure screenshots.
- [x] 8.4 Share normal application close decisions and expose them with safe response/exit sequencing.
- [x] 8.5 Run focused regressions and real attachment acceptance, update contracts and complete independent re-review.
