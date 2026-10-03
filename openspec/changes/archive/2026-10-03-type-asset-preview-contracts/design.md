## Context

Asset preview requests/results belong to Runtime/AssetEditing; Editor implements IAssetPreviewWorkspace and both GUI and automation use its existing setting mutation path. FAssetPreviewSettings exposes optional uint32 shape/channel fields. Editor stores these as indices, repeats shape names for geometry lookup and Combo labels, validates fixed maxima, and derives a texture component with channel - 1. Sky references and displayed textures are paired through three parallel positions; format captions use ETextureFormat ordinals.

## Goals / Non-Goals

**Goals:**
- Make internal option identity, wire identity and GUI position independent.
- Keep one authoritative shape/channel mapping and one named sky product association.
- Preserve the existing schemas, wire values, defaults, labels, option order, images, errors' categories, history/generation semantics and ownership.
- Prove correspondence with fixed expectations, reordered/relabelled presentation tests, real shared-workspace tests and attached automation.

**Non-Goals:**
- No new preview controls, shapes, channels, image algorithms or performance work.
- No changes to texture encoding edits, cube face storage conventions, asset formats, lifecycle orchestration or other review items.
- No generic option framework, plugin changes, archive, commit or push.

## Decisions

### AssetEditing owns typed preview option descriptors

Add EAssetPreviewShape and EAssetPreviewChannel plus immutable descriptors in an AssetPreviewOptions header/implementation. Shape records bind identity, existing uint32 wire value, caption and full engine model path. Channel records bind identity, existing wire value, caption and optional RGBA component index. Strict lookup and parse helpers reject unknown identities/values. GUI indices are resolved by searching identities and reading the selected descriptor, never by enum casts or index arithmetic.

Keep FAssetPreviewSettings fields and reflection declarations unchanged as the compatibility boundary. SetPreviewSettings and SetTexturePreview decode before mutation; FEntry/FTextureView and worker captures carry enums. Output encodes through the same descriptors. Retain existing same-value checks so redundant choices do not invalidate previews. This avoids an enum-reflection/schema migration and preserves older clients.

### Presentation helpers are private to Editor

Small preview-specific helpers map a supplied descriptor span between typed identity and GUI index/labels. They can be tested with reordered and relabelled copies. Consumers resolve resource paths and channel components from the authoritative contract after selection. No changes to RasterOptions or generic Gui APIs are required.

### Named sky product storage replaces parallel arrays

Editor owns FSkyPreviewProducts with Radiance, Specular and Brdf texture members. A single private descriptor table associates a caption, an FSkyAsset reference member and an FSkyPreviewProducts texture member. Preparation and display traverse those same associations. These are preview associations, so they do not become a new Environment runtime registry. Async preparation still returns owned shared_ptr values and publication copies the same owned result.

### Format display uses explicit identity lookup

Preview format metadata maps ETextureFormat explicitly to the existing long and compact captions, sharing them across texture and sky properties. This is display metadata owned by the preview layer; texture layout, serialization and format encoding stay with Textures.

### Keep changes focused and verifiable

Reuse the existing shared workspace service and tests. Add focused CPU contract tests and Editor-private mapping tests; extend workspace/automation checks for all legal values, invalid requests, unchanged settings, no asset history changes and preview readiness. Freeze the old reflected wire/schema representation before production edits. Exercise existing asset editor, workspace and parity regressions in Debug/Release, then run formatting, naming and dependency checks. Existing AssetWorkspace.cpp exceeds 500 lines but is unchanged here; its header adopts typed state, and broad lifecycle splitting remains a separate task under the documented migration rule.

## Risks / Trade-offs

- [Accidental wire/schema changes] -> Leave public integer fields/reflection intact and compare a fixed baseline plus real discovery/invocation.
- [GUI position treated as identity] -> Test reordered/relabelled descriptor spans and fixed geometry/component expectations.
- [Sky products silently exchanged] -> Verify distinct reference and texture sentinels for every member association.
- [Different pixel conversion] -> Change only component selection; retain the conversion/compositing algorithm and verify selected-channel pixels.
- [State partially mutates on invalid options] -> Decode before mutation and assert settings/generation/dirty/history remain unchanged after rejection.
- [GPU/desktop tests require host permissions] -> Use the repository's bounded test commands and record any actual environment failures without weakening tests.

## Migration Plan

Capture compatibility baseline, add descriptors and private presentation/product helpers, migrate consumers, then run focused and existing regression coverage. No saved data migration is needed. Reverting this change restores the previous implementation because external schemas and assets remain compatible. Leave the active change unarchived and the worktree uncommitted after validation.

## Open Questions

None blocking implementation.
