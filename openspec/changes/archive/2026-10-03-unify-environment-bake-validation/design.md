## Context

ImportSettings and BakeEnvironment duplicate radiance/specular/sample admission rules. PrefilterEnvironment has the same specular/sample policy but may consume a renderer-captured cube larger than the panorama bake limit. Import-format/type validation and panorama/pixel validation belong to different layers.

## Goals / Non-Goals

**Goals:** Environment owns numeric limits and settings predicates, all relevant consumers agree, existing behavior and reflected metadata remain stable.

**Non-Goals:** No new settings, changed ranges, image algorithms, import publication, plugin lifecycle, transport behavior, archive or commit. BRDF integration has a separate quality policy and remains outside this change.

## Decisions

- Add FEnvironmentBakeLimits and IsValidEnvironmentBakeSettings to the existing CPU Environment public contract. A predicate lets each caller retain its existing invalid_argument message and validation precedence; a shared throwing validator would unnecessarily change those boundaries.
- Add IsValidEnvironmentPrefilterSettings for the numeric subset (source size, output size, samples). Full bake validation calls this subset plus the panorama-specific radiance rule. Do not synthesize a full bake request for standalone prefiltering, which would incorrectly cap captured input size at 1024.
- BakeEnvironment retains panorama dimensions/storage and finite/radiance checks; PrefilterEnvironment retains texture/cube/linear checks; AssetImport retains extension/type and texture-encoding checks. Only shared numeric admission moves.
- Derive import field descriptions and its existing range diagnostic from the domain limits/defaults while preserving exact emitted text, field IDs, versions and default values. No enum or reflection migration is involved.
- Add fixed-case boundary tests for the predicates and import preflight, invalid settings passed to the real baker, small valid baking, source-format/type rejection, prefilter's larger-source contract, and literal reflected metadata expectations. Retain the original preprocessing/import workspace/attached import regressions.

## Risks / Trade-offs

- [Accidentally broadening or narrowing acceptance] -> fixed min/max, non-power-of-two, zero, overflow and cross-field cases; retained messages and unchanged validation order.
- [Applying panorama constraints to captured cubes] -> direct prefilter-policy test with source width above 1024.
- [Schema/documentation drift] -> literal metadata/default checks plus existing attached import discovery/invocation tests.
- [Testing maximum sizes incurs excessive baking cost] -> validate boundary settings without allocating full images; execute only small accepted bakes and immediate rejection cases.

## Migration Plan

Capture baseline, implement the small shared contract, migrate consumers, validate Debug/Release and style/boundaries, and record evidence. No saved-data migration. Revert only this change's files/hunks if rollback is needed; keep the existing preview-contract change intact.

## Open Questions

None.
