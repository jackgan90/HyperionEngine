## ADDED Requirements

### Requirement: Authoritative importer descriptors

Import discovery, request type/extension/option validation and conversion selection SHALL derive from registered importer descriptors. Descriptors MUST declare stable importer/output identity, version, normalized extensions, supported conversion options and workspace/tooling exposure. Startup registration MUST reject malformed or conflicting entries without adding the rejected descriptor, and MUST stop before work is admitted.

#### Scenario: Existing output type gains a test source
- **WHEN** a host selects a test converter for an existing output type
- **THEN** capability discovery, validation, GUI filters and actual conversion use its descriptor without a central extension list edit

#### Scenario: Application startup selects or disables importers
- **WHEN** RegisterAssetServices receives a descriptor collection in its startup options
- **THEN** the published workspace and automation discovery/invocation use exactly that frozen collection
- **AND** an explicit empty collection disables source imports while unset options preserve built-in capabilities
- **AND** malformed descriptors produce a controlled asset-plugin startup failure and unavailable import operations without disabling unrelated automation

#### Scenario: Startup composition uses a local reflected descriptor
- **WHEN** asset services are registered with a caller-owned reflected descriptor before delayed plugin startup
- **THEN** registration retains its metadata so later caller changes or destruction do not alter the selected importer
- **AND** full validation and freezing still occur before the workspace is published

#### Scenario: Invalid or late registration
- **WHEN** an entry has malformed/duplicate extensions, a conflicting importer identity or type/extension pair, or registration is frozen
- **THEN** registration fails and existing entries remain usable

### Requirement: Explicit tooling exposure and unchanged default contracts

Default workspace capability values, schemas and supported formats SHALL remain unchanged. ToolingOnly descriptors MUST be excluded from GUI/capability projection but remain selectable by an explicit supported output type. Instance consumers MUST describe the actual selected converter set.

#### Scenario: Default glTF source interpretation
- **WHEN** default capabilities are queried or an explicit tooling model-source conversion is requested
- **THEN** capabilities show the existing Model/Texture/Sky formats and tooling conversion remains available without becoming a GUI choice

### Requirement: Preserve import admission and publication semantics

Descriptor-driven validation SHALL retain existing output containment, generation, source identity, settings and dirty/busy checks. Admitted import/publication tasks MUST retain their existing noncancellable completion, provenance, rollback, history and content-root retirement semantics.

#### Scenario: Rejected request or failed publication
- **WHEN** a request is stale, outside Game, invalid for the converter or fails during publication
- **THEN** shared validation and rollback reject it without partial publication or false success

#### Scenario: Custom texture importer receives an explicit name
- **WHEN** a converter produces FTextureAsset and the request supplies a name
- **THEN** direct publication, draft preparation and publication of that draft retain the requested name independently of importer identity
- **AND** an empty requested name retains the converter default without mutating cached conversion objects
