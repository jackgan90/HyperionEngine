# Verification — named Renderer cache keys

Baseline: `05d572cce5dd9ceb43848a86c132ee91f604dcd5`; verified on 2026-10-04 with the companion GUI/shadow changes.

## Contract coverage

- `RenderCacheKeyTests.cpp` compares named keys against the legacy tuple model for equality, lexicographic ordering, exact hash mixing, view/usage ranges and request retry progression.
- Resource pointer ordering still delegates to the same tuple comparison. Designated construction and named fields replace positional access at consumers.
- Existing instance/material/resource tests cover reuse, invalidation, generations, old-frame retention, failure retries and release. No cache algorithm, key dimension, payload ownership or fence policy changed.

## Validation

Debug and Release full builds passed. All 32 selected Debug regression tests passed (`out/TypedMaintenanceDebugTests.log`), including the Debug-only cache-allocation failure test.
All 31 selected Release tests have passing results, including `render_cache_keys`, `instance_batching`, `render_resources` and `material_rendering`; the companion GUI fixture passed its targeted rerun. Evidence: `out/TypedMaintenanceReleaseTests.log` and `out/TypedMaintenanceGuiReleaseRetest.log`.

Full formatting/path checks, semantic naming for 26 changed C++ translation units, module boundaries, diff checks and strict OpenSpec validation passed. Modified production files were manually checked against the 500-line limit and semantic maintainability rules.

## Independent review and delivery

The reviewer confirmed original key field order, hash order, view prefix retirement, retries and lifetime semantics. TM-R1 identified missing dependencies from `hyperion_check` to the new key/material-choice tests. The dependencies were added, both presets regenerated successfully, and the reviewer confirmed the finding closed from the generated build graphs.

Implementation and review were completed without archive or commit. Following acceptance, the user authorized spec synchronization, archive and a local Git commit on 2026-10-04. No push is included in this delivery.
