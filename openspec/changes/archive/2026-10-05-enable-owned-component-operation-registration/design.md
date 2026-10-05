## Context

Scene owns component storage, reflection and change facts. SceneEditing already provides typed get/edit/batch requests, field validation, atomic document commits, revision checks and history. Automation currently wraps those operations through private templates and lists eight built-in types centrally. Catalog registration runs on Main before Seal; plugin cleanup withdraws owner closures before providers are destroyed.

## Goals / Non-Goals

Provide a public feature-owned typed adapter with explicit owner and read/write exposure, preserve built-in protocols and reuse existing editing semantics. Do not make Scene depend on Automation, automatically expose every reflected type, change field permissions, introduce hot loading or add component cases to transports.

## Decisions

### Public typed adapter belongs to AutomationHost

Expose `RegisterSceneComponentOperations<T>` in the Automation plugin's public interface. The feature first registers its CPU component descriptor, then its automation adapter during startup. Registration options require an owner; reads are enabled by default and writes require explicit opt-in. An optional typed example supports components whose valid defaults need authored values. Built-ins explicitly retain read/write registration under their existing owner.

Reuse the common typed wire/error adaptation rather than copying domain editing into the adapter. Requests/results retain existing reflection descriptors and operation names. Registration validates that the registered scene component and `RecordType<T>()` are the same authoritative type.

### Startup registration and lifetime are explicit

Preflight Main ownership, Seal and selected operation identities before registering the family, so duplicate or late requests do not add a partial family. Validate typed examples before publishing operations. The host/provider and record descriptors outlive callable registrations. The feature installs scoped plugin cleanup calling `UnregisterOwner` before destroying its document/provider. Missing providers remain discoverable with the existing unavailable metadata and controlled invocation error. Synchronous calls commit on Main; this adapter adds no asynchronous provider captures.

### Exposure is independent of edit validation

Read-only registration does not publish set/set_batch. Exposed writes still pass through SceneEditing's component edit policy and complete candidate validation, idle/document/revision checks, atomic batch commit and history. The adapter does not override immutable fields or grant general transport mutation permission.

## Risks / Trade-offs

- A provider destroyed before owner withdrawal would leave borrowed pointers → document scoped cleanup, test withdrawal and existing plugin shutdown ordering.
- Registration could partially expose a family on duplicate/invalid examples → preflight selected identities and examples before publishing.
- Moving templates may change metadata/schema → reuse existing request/result descriptors and test built-in discovery/invocation alongside external components.
- New output/render adapters might be confused with component registration → keep Renderer/Editor adapters separately owned and mark unprovided external adapters as deferred in the coverage inventory.

## Migration Plan

Extract the wire helper and component adapter, migrate built-ins, add an independently registered test component, then verify editing/discovery/lifecycle and update extension documentation. Existing data requires no migration.
