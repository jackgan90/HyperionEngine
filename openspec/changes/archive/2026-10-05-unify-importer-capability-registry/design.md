## Context

FAssetImporter already binds output reflection, source extensions and converter. Service registration stops at the first conversion, while Workspace repeats supported formats and a tooling-only glTF exception. GUI and automation query a static capability list rather than the selected instance.

## Goals / Non-Goals

Use one descriptor source for conversion selection and capability projection; support host-selected startup descriptors and preserve default format/protocol behavior. Do not add production formats, hot loading, new output asset kinds or change publication/rollback, task cancellation, history or root protection.

## Decisions

### Descriptors own visibility and presentation

Append explicit Workspace/ToolingOnly exposure, capability description and named texture-encoding/sky-bake support to FAssetImporter. Keep output identity in its authoritative reflection descriptor. Built-in descriptor factories provide the existing converter, ID/version, extensions, option support and description together; compatibility Register helpers use those factories. The old extension/type settings validator delegates through default descriptors; actual requests use the selected descriptor, so an existing output type's test format needs no second extension list. Default workspace descriptors keep Model/Texture/Sky order and the tooling-only FModelSource converter.

Service registration normalizes extensions, rejects malformed/duplicate entries, duplicate importer IDs and duplicate type/extension pairs before admission. Different output types may share a source extension, preserving glTF model/source interpretation. Descriptors and captured providers outlive admitted work; record descriptors are retained as owned immutable copies with their original definition identity.

### Selected service set drives consumers

Workspace accepts a descriptor set at construction, registers it and freezes registration before exposing the workspace. RegisterAssetServices passes FAssetServiceOptions.Importers to that constructor: an unset optional selects built-ins, while a supplied collection replaces the defaults, including an explicit empty collection that disables source conversion. Registration retains each non-null reflected type descriptor before delayed startup so subsequent changes or destruction of the caller's descriptor do not affect the captured selection. Complete validation stays at plugin Start. Invalid selections fail asset-plugin startup through the existing controlled plugin failure policy; optional automation remains available and reports the missing provider. Converter callbacks and borrowed providers must outlive admitted work, which the plugin drains before teardown. Its instance capabilities project only Workspace descriptors, grouping extensions by stable output type. Validation resolves actual descriptors, with explicit ToolingOnly selection permitted for existing tools; inferred type selection uses Workspace descriptors. GUI choices/filters and automation query the instance. The static compatibility capability function projects the default descriptor set through the same path.

Conversion and publication use the same normalized descriptor lookup. Preserve existing wire DTO/schema, caller-requested type and publication provenance. Accepted imports remain noncancellable and retain content-root, dirty/busy and path containment checks.

### Requested names follow the output asset contract

Direct publication and draft preparation share one private helper that applies an explicit name by reflected C++ output type. FTextureAsset receives the name independently of importer identity; FModelAsset receives it for model imports, while scene imports keep the requested name on their scene instance. Empty names preserve converter defaults, other output kinds keep existing behavior, and prepared publication retains draft edits without renaming again. Copy the immutable converted value before changing its name so cached conversion results remain unchanged.

## Risks / Trade-offs

- Capability ordering could change GUI defaults → preserve default projection order and stable typed selection tests.
- New descriptors could be accepted by validation but absent from conversion → both paths use the frozen service descriptor set and a shared selector.
- Startup descriptor lifetimes could expire → copy metadata/reflection into owned storage, document callback-provider lifetime and reject late registration.
- Tooling conversion could become a GUI option → explicit exposure filtering with a separate explicit-type validation path.

## Migration Plan

Extract built-in factories and descriptor helpers, migrate service/workspace and consumers, add an existing-type test format and negative registry tests, then build/test/audit. Native assets and persistent import settings require no migration.
