## Context

AssetImport already owns worker conversion, identity, provenance and transactional publication. FImportAutomation privately owns orchestration and imports, so Editor cannot share its task state. PNG/JPEG decoding and texture mip construction exist, while sky bake options are currently supplied only by recipes. Platform exposes a folder picker but no file picker.

## Goals / Non-Goals

**Goals:** One import domain service for Editor and automation; configurable existing import paths; standalone PNG/JPEG textures; explicit sky settings; bounded observable asynchronous tasks; preserved incremental and content-root contracts.

**Non-Goals:** New native types, model formats beyond glTF/GLB, cubemap face inputs, compression, new bake algorithms, download tools, library migration UI, undo of published files, accepted-job cancellation, live plugin loading, automatic replacement of open drafts, archive or commit.

## Decisions

1. Put reflected request/result, validation and a Main-only import workspace in Runtime/AssetImport. The existing assets service plugin owns and publishes the workspace alongside IO, Assets and Content, updates it even when minimized, registers it as a scoped content-root participant and drains it before IO destruction. This uses an existing provider rather than introducing a lifecycle for every feature. Runtime/Assets remains independent of source import. Editor and Automation declare typed optional consumption; transport and Runtime/Application stay unchanged.
2. Retain asset.import and its existing input keys/type IDs/output fields. Add optional texture encoding and sky settings. Register import capabilities, validation and application-scoped task queries. GUI calls the same workspace directly. Session jobs retain their current isolation; workspace task IDs are separate, bounded, invalidated across root changes and queryable across GUI/agent producers. Poll closures retain owned task records even if completed history is pruned.
3. Pass typed conversion settings into the root conversion context, including direct HDR/EXR and sky recipes; explicit sky options override recipe settings, omitted options preserve source/default behavior. Keep dependent asset conversion semantics unchanged. Settings are included in publication provenance. Existing name behavior remains documented; standalone textures use the requested name or source stem, while sky names use the recipe/source name.
4. Register PNG/JPG/JPEG to FTextureAsset using DecodeImage and BuildTextureAsset. Default standalone images to sRGB and allow explicit Linear. The full mip chain, RGBA8 layout and existing validation remain authoritative. The same registration and option path is available to AssetTool, without new hasset types.
5. Editor adds a non-modal Import Asset window with source/type, applicable settings, output/library, advanced source identity/force/root ID and status sections. Default GUI library is /Game; old API library defaults remain output parent. GUI requires a writable selected Game output; the domain retains explicit local-path compatibility for existing automation/tools. A Platform file picker uses engine-owned filter data and private Windows COM implementation.
6. Domain validation performs no publication and checks generation, path/option compatibility, source presence and output mount permissions. Conversion, graph and identity validation remain authoritative during publication. Successful publication refreshes the index and increments a completion revision observed by Editor to refresh Content Browser. Index refresh failure after a committed publication is reported explicitly as a warning, so callers do not mistake committed files for a rolled-back import.
7. No cancellation or percentage progress is invented. Closing the panel leaves tasks alive; shutdown drains them. Busy tasks prevent root changes; clean root changes clear the workspace history and invalidate old task IDs. Disk publication has no scene undo history, and opened drafts retain their normal conflict handling.

## Risks / Trade-offs

- Changing conversion settings without provenance would silently skip reimport -> record all explicit settings, validate type applicability and test unchanged versus changed inputs.
- Source or output can change after validation -> final worker publication retains leases, source fingerprints and identity checks.
- New importer registration changes recorded importer settings -> an existing asset may require a one-time freshness rebuild; byte-identical files still avoid rewriting.
- Main-side index refresh can be expensive -> retain the existing scan behavior in this version rather than introduce an unrelated index optimization.
- File pickers are native modal dialogs -> open them from the existing Main control boundary, not inside rendering; automation supplies target-accessible paths without opening dialogs.
- Long-running work and failed startup can retain captures -> scoped root deregistration and owner cleanup; accepted work drains before provider destruction.

## Migration Plan

No native schema migration. Additive API fields preserve omitted-field behavior. Existing CLI switches and asset.import stay supported. Update docs and regression coverage together. Rollback is a source revert; published assets remain existing native types.

## Open Questions

None blocking. Pure panel layout/visibility remains presentation, consistent with the accepted plan and current automation coverage policy.
