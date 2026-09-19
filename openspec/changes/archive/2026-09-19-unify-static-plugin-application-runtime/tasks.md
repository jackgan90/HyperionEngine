## 1. Static runtime

- [x] 1.1 Implement startup planning, optional ordering, explicit disablement, service requirements and tolerant diagnostics.
- [x] 1.2 Implement owner-thread updates, scoped services/events/cleanup and quiesce-before-stop lifecycle.
- [x] 1.3 Add meaningful runtime tests for unavailable branches, partial startup, ordering, service scopes, event lifetime and teardown.

## 2. Application composition

- [x] 2.1 Add the minimal Application host and empty-host coverage.
- [x] 2.2 Add reusable plugin-owned asset, window, graphics/session and GUI services with application-supplied native providers.
- [x] 2.3 Move Viewer orchestration into an application feature plugin and integrate RenderDoc into its unified startup graph.
- [x] 2.4 Replace concrete scene routing with shared input/update/status interfaces and move delta-time advancement into scene plugins.
- [x] 2.5 Move Editor into the shared application host and service graph, preserving document/history/viewport behavior and non-drawable background progress.

## 3. Feature selection and extensions

- [x] 3.1 Separate activation from persisted feature values; add explicit disablement and optional build/catalog selection.
- [x] 3.2 Add render-stage/resource extension contracts and migrate contact shadows to an optional feature registration.
- [x] 3.3 Add scoped GUI/contribution hooks and retain existing editor and viewer panels through those contracts.

## 4. Verification and documentation

- [x] 4.1 Update integration coverage for disabled/missing plugins, stale source settings and application profiles.
- [x] 4.2 Run relevant CPU/GPU/Editor acceptance, disabled-feature build checks, style, naming, boundaries and strict OpenSpec validation; resolve failures.
- [x] 4.3 Update architecture/source-layout/extension documentation, require the plugin contract through AGENTS.md and contributor entry points, and record validation evidence; leave all changes uncommitted.
- [x] 4.4 Check final GPU validation in the graphics owner after dependent resources and swapchain cleanup, add late-error injection coverage, and independently review the change.
