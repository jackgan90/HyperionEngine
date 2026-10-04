# Verification — typed shadow preview modes

Baseline: `05d572cce5dd9ceb43848a86c132ee91f604dcd5`; verified on 2026-10-04 with the companion GUI/key changes.

## Contract coverage

- `ShadowPreviewTests.cpp` freezes every directional value 0–5 and contact value 0–2, their labels and cascade mappings. Unknown numeric values, sentinel identities and signed overflow are rejected.
- Archive and wire round trips preserve `debugMode` as an unsigned 32-bit field without enum schema metadata, and preserve record IDs/versions.
- Config regressions exercise invalid legacy `contact_shadow_debug` values through setters, JSON load/save, records and wire reads without partial mutation.
- Existing shadow/contact/Deferred GPU tests and Editor render acceptance cover both depth conventions, authored/session defaults, GUI/automation parity, feature disablement, unavailable operations and restart persistence.

## Validation

Debug and Release full builds passed. All 32 selected Debug regression tests passed (`out/TypedMaintenanceDebugTests.log`), including the Debug-only cache-allocation failure test.
All 31 selected Release tests have passing results; the only initial failure was the companion GUI click fixture, which passed after a test-only correction. Evidence: `out/TypedMaintenanceReleaseTests.log`, `out/TypedMaintenanceGuiReleaseRetest.log`, and the Debug/Release build logs.

Formatting, semantic naming for changed C++, module boundaries, diff checks and strict OpenSpec validation passed. Scene consumes the standard-library-only RasterOptions module directly; no Renderer/RHI dependency was introduced.

## Review and delivery

Independent review found no shadow protocol, behavior, ownership or lifecycle regression. The shared TM-R1 test-build dependency finding was fixed and its generated Debug/Release rules rechecked. No unresolved confirmed findings remain.

Recorded implementation and regression checks were completed before archive or commit. Following acceptance, the user authorized spec synchronization, archive and a local Git commit on 2026-10-04. No push is included in this delivery.
