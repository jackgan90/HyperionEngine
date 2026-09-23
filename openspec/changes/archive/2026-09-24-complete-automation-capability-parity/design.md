## Context

The existing automation endpoint already supports typed operation registration, lazy discovery, strict wire/schema conversion, Main-thread execution, asynchronous jobs and attachment to Editor/Scene Viewer. Scene documents and asset documents already own validation/history/persistence. Remaining gaps are domain adapters and GUI-private orchestration, not transports.

## Goals / Non-Goals

**Goals:** Existing human tasks are achievable through discoverable CLI/MCP operations; GUI and agents mutate the same live documents; new features need a small domain registration and parity test. Preserve dirty, revision, busy, save, cancellation and shutdown semantics.

**Non-Goals:** Export every Public C++ method, emulate GUI gestures, add engine features, replace the catalog/session/transport/reflection framework, implement network authentication or new platform providers, hot-load plugins, add general event subscriptions or binary artifact streaming. Panel docking and window placement are presentation arrangements; content, camera, rendering and persistent preference changes are task capabilities and remain in scope.

## Decisions

### Development order and completion gate

1. Inventory and contract baseline, then scene document CRUD/selection/settings/components/hierarchy. These exercise existing history and establish reusable reflected editing.
2. Document/root orchestration and content discovery. Explicit save/discard/reject policies replace modal prompts for agents while calling the same transition service.
3. Asset workspace/document services and property/reference/encoding edits. Attach to the actual GUI documents before exposing live asset operations; then share domain editing with standalone automation.
4. Host view/camera/render settings, previews, capture status and result files. Distinguish Main state commit from resource readiness and file completion.
5. Existing import/publication workflows, followed by end-to-end parity acceptance and documentation.

Each task family records its actual operation, availability and evidence in a coverage inventory. Completion requires every in-scope existing task to be covered; unavailable providers are not a substitute for an unfinished adapter.

### Domain ownership and typed services

Keep Runtime/Application unchanged. Define focused value contracts and service interfaces in the existing owning domain modules. Editor/Viewer publish services through the plugin service mechanism; Automation binds them without concrete-plugin casts or private cross-plugin headers. Where a GUI method mixes widgets with business logic, extract the latter and route both consumers through it. Do not introduce a generic string-command host dispatcher or a second asset workspace in attached mode.

### Reflected editing and registration

Use reflected scene components and asset records for field shape/schema and value conversion. Stable document/handle identity and expected revision/generation remain explicit. Validate the final candidate through the existing domain service, including cross-field and reference/resource constraints; Inspector visibility alone does not grant write access. Multi-target edits validate all targets before publication and produce one history transaction. Reference changes retain asynchronous loading, type/dimension validation and stale-generation rejection. Bulk payloads, internal identity and native handles remain excluded.

Prefer typed DTOs and small descriptor-driven registration helpers over handwritten schemas or JSON-encoded strings. If a reflected dynamic record needs a limited contract supplement, constrain it to the domain's known descriptors and strict existing wire conversion; do not add arbitrary script execution or unvalidated JSON patching.

Material value/type records are recursive. Schema generation retains existing inline shapes for acyclic records and emits a local JSON Schema `$ref` on a recursive edge, pointing to the earlier inline descriptor. Actual wire values keep their existing bounded depth/node/byte limits. This is a schema projection correction, not a new serialization or execution framework. Member-pointer registration derives property request/result types directly from the existing C++ field types.

### Live coordination

Expose selection and active document identity explicitly. Active GUI interactions, saves, pending references and root/document retirement participate in admission checks. Scene/root replacement invalidates old handles and jobs according to existing lifecycle contracts. Dirty decisions are explicit request values, never implicit discard. Attached mutations invoke target services, use target paths and update target preferences. GUI controls consume the same decision/action methods.

### Views and results

Expose serializable camera state and the existing host settings; preserve temporary browsing state versus persistent scene settings. RenderDoc remains optional with an actionable unavailability reason. Reuse existing capture/readback mechanisms and jobs. Return bounded metadata and target-local file paths only after the promised completion point; no raw texture data in JSON. Clients can distinguish accepted edits, prepared resources and completed captures.

### Compatibility and lifecycle

Keep existing IDs and field meanings. Add operations and optional fields only; describe host-specific capabilities truthfully. Register callbacks with scoped owner cleanup and drain pending work before destroying providers. Transports do not gain domain branches. Keep CPU services testable without graphics and use real attached applications for visibility/history/persistence parity.

## Risks / Trade-offs

### Audit corrections

Default scene component creation shares GUI admission and excludes prepared model bindings. Viewer explicitly selects the first remaining model after deletion; Editor retains clear-selection behavior. Typed per-component `set_batch` operations accept parallel handle/instance/value arrays, validate every candidate and commit through the same document transaction as multi-selection Inspector.

Workspace metadata includes loading/failed entries with generation zero. These entries can be activated and closed, then reopened to retry without resetting unrelated documents. ModelViewer publishes temporary viewport controls without requiring an authored scene document; it uses empty document and zero revision. Viewer main-light controls share one atomic domain method with the shadow panel. Producer errors take precedence in diagnostics; screenshots require drawable output rather than ready content.

Application close is a host-owned typed service in Runtime/Config alongside application settings; Runtime/Application remains unchanged. Request acceptance is synchronous, save-before-close continues in the host, failures leave the target open, and Cancel cancels exit rather than an admitted disk write. Successful exit disconnects clients and withdraws discovery. The connection server gains only bounded draining of already queued replies on StopAdmission (two seconds maximum for a stalled peer); session jobs retain their existing drain policy. No domain operation is added to transports and no remote delivery acknowledgement is promised.

- GUI-private state can conceal business rules → inventory callbacks and extract shared methods before registration; test both consumers.
- Reflection can expose invalid edits → preserve domain validation, immutable fields, reference preparation and whole-batch atomicity.
- Multi-client edits can race GUI interactions → expected revisions/generations and busy checks remain mandatory.
- Captures and saves can outlive documents → owned snapshots, existing task tracking, explicit completion and shutdown tests.
- Scope spans many existing features → deliver in dependency order with unchecked tasks retained until verified; no coverage claims based only on schema count.

## Migration Plan

Add domain services and adapters incrementally, keep existing GUI behavior and operation IDs, update composition only when a provider is ready. Run focused regressions after each slice, then full affected builds and real CLI/MCP workflows. No persisted asset migration is intended. Reverting the change restores the earlier adapter subset without changing asset formats.

## Open Questions

No product decision blocks implementation. Concrete operation grouping and descriptor helpers will be chosen against the actual owning services and recorded with their tests; any need for framework-level redesign is outside this authorization and must be reported before proceeding.
