## Context

`SplitModelSource` converts CPU source materials into independent native material and texture products. The five source bindings are currently paired with texture, sampler and UV semantics through array positions; positions also select sRGB interpretation. Material value arrays participate in native content revisions, and texture encoding participates in product sharing.

## Goals / Non-Goals

**Goals:** Centralize each source-role mapping, make sampler interpretation explicit, and preserve both semantic output and persistent content/identity when descriptor rows are reordered.

**Non-Goals:** New texture roles, source-format support, changed PBR/shader contracts, new automation operations, general metadata registries, or publication/lifecycle changes.

## Decisions

### AssetImport owns a private role description

Each row contains a typed `FModelMaterial` texture member pointer, existing texture/sampler/UV semantic enums, a texture encoding and an explicit output order. A validated ordered view drives conversion. Position in the descriptor declaration has no semantic meaning. Materials continues to own target semantic definitions; Scene retains its CPU source representation and existing serialized values.

Unlike parallel arrays or a new global registry, this keeps the complete source-to-target association at one small conversion boundary. Invalid duplicate/missing output orders and duplicate source/target identities are rejected at the private ordering boundary; the built-in table is checked at compile time.

### Preserve historical output and texture sharing

Order roles explicitly as BaseColor, MetallicRoughness, Normal, Occlusion and Emissive, preserving texture/sampler/UV entry order within each role and existing numeric factor order. Preserve named product keys, source-derived shared keys, material slots and image-plus-encoding deduplication. Missing images still use one Linear white texture; missing samplers retain `FModelSampler` defaults. `bHasNormal`, queues, factors and subresource IDs remain unchanged.

Arbitrary name sorting or merely preserving runtime values would alter serialized arrays and revisions. An explicit output-order field permits declaration reordering without changing persisted content. No importer version bump or asset migration is needed when compatibility verification succeeds.

### Sampler rules are exhaustive and explicit

Use switches over all supported source Min/Mag filters and wrap modes. Min filters independently specify min filtering, mip filtering and whether MaxLod is zero or its current default. Keep all other material sampler fields at their existing defaults. Unknown enums remain failures; do not add default-value fallback or change source enum numbers.

### Verify source mapping independently and keep private tests private

First add public-boundary tests with fixed source bindings and independent literal expectations, including fingerprints captured from the pre-refactor converter. Exercise all Min/Mag/U/V combinations, all five roles, UV0/1, missing resources, same-image encoding variants, default materials and invalid source references. A module-private test translation unit exercises permutations of the private descriptors through the same conversion path without exposing descriptors in Public or letting tests cross private-header module boundaries.

Run affected import/publication, D3D12 model rendering and existing GUI/CLI/MCP import regressions, Debug/Release builds, formatting/naming, boundary checks and strict OpenSpec validation. GUI and automation keep the shared `FAssetImportWorkspace` service and existing discovery, schemas and task lifecycle.

## Risks / Trade-offs

- **Array reordering changes revisions** → preserve historical output order and compare pre-refactor fingerprints plus encoded bytes under descriptor permutations.
- **Encoding changes split or merge shared resources** → independently verify image/encoding keys, source sharing, white fallback and published graph identities.
- **Tests share an incorrect table** → fixed source-role and sampler expectations do not derive expected values from production descriptors.
- **Private test seam becomes a public API** → keep role descriptions and the reordered conversion entry private; compile private regression code only into the test executable.
- **Extra import work** → order a five-row description once per split, outside material iteration; no per-frame work or new resource lifetime is introduced.

## Migration Plan

Capture compatible output on the unchanged converter, introduce the private mapping and explicit sampler conversion, run regressions, and update the asset pipeline documentation. Existing native files remain readable and unchanged imports remain current. Rollback restores the converter without migrating assets.

## Open Questions

None required to begin implementation.
