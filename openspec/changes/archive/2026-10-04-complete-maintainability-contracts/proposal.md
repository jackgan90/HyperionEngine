## Why

The remaining maintenance work still duplicates reserved asset names, decodes render options repeatedly, uses free-form error strings inside domain control flow, and infers silhouette support from shader paths. Complete roadmap items 4, 9, 10 and 12 as one user acceptance batch, preserving the existing behavior and external boundaries while making their owners explicit.

## What Changes

- Define Assets-owned reserved entry names and publication staging prefix; preserve browser case folding and migration-specific exclusions.
- Carry typed pipeline, GBuffer, visualizer, shadow-preview and viewport choices through runtime/configuration state. Define profiling HUD categories as typed flags, preserving numeric/string configuration, reflection and automation representations through explicit boundary adapters.
- Define stable typed error identifiers in their owning domains, retain unknown external codes, and consolidate shared automation failure conversion across synchronous and asynchronous calls.
- Centralize extensible material usage/variant names in Materials and replace outline-time shader-path inference with a declared silhouette policy. Version persistent material passes and migrate old passes with their historical eligibility; keep old wire inputs compatible through an explicit legacy default at the decode boundary.
- Verify the full batch with compatibility fixtures, domain and GPU/GUI/automation regressions, production build selection and a fresh independent quality audit before the single acceptance stop.

## Capabilities

### New Capabilities

- `reserved-asset-entry-contracts`: Shared reserved names with caller-owned filtering policy.
- `typed-runtime-option-state`: Typed internal options with compatible configuration and protocol adapters.
- `domain-error-code-contracts`: Domain-owned stable error identities, opaque-code preservation and shared failure translation.
- `material-usage-contracts`: Extensible well-known material usage and variant names.

### Modified Capabilities

- `material-assets`: Explicit persisted silhouette policy with compatible legacy material-pass migration.
- `selection-outlines`: Resolve coverage from the declared material policy while preserving custom masks and old asset behavior.

## Impact

Assets, Config, RasterOptions, Scene, Renderer, Materials, domain error owners, Automation adapters and their Editor/DebugUI/Triangle consumers; focused shared Core/Reflection helpers; related tests and documentation. No plugin lifecycle, native backend algorithm, operation ID or unrelated resource ownership change. Existing render/configuration wire shapes stay stable. The material-pass policy is an explicit compatible optional wire extension and versioned native-record migration. Full oversized-file cleanup, Renderer directory reorganization and Editor responsibility/Endpoint restructuring remain in the user's later three batches. The already implemented close-observation change remains uncommitted for the same acceptance point; the excluded user draft is untouched.
