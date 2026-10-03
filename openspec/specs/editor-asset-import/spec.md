# editor-asset-import Specification

## Purpose
Expose configurable external asset import through Editor and automation using shared validation, task lifecycle and transactional publication.
## Requirements
### Requirement: Dedicated import panel
Editor SHALL provide File > Import Asset opening a non-modal panel with clearly separated source, conversion settings, output, advanced options and task results. It SHALL support glTF/GLB, independent PNG/JPEG, HDR/EXR skies, existing typed JSON/sky recipes and native upgrades without adding native types. File selection SHALL use the Platform wrapper. GUI publication SHALL require a writable Game destination.

#### Scenario: Import a model from File
- **WHEN** a user opens Import Asset, selects a glTF/GLB source, configures its output and starts import
- **THEN** Editor remains responsive and displays the resulting asset, written count, unchanged state or actionable failure

#### Scenario: Missing writable content root
- **WHEN** Game is unset or read-only
- **THEN** the panel explains the restriction and does not publish

### Requirement: Shared configurable import domain
GUI and automation SHALL use one UI-independent domain for validation, accepted tasks, publication, results and index updates. Existing asset.import keys and behavior SHALL remain compatible, with additive optional settings. Capabilities, validation and task state SHALL be reflected and discoverable without transport-specific branches. Task records SHALL be bounded and separate from session job identifiers.

#### Scenario: Agent and GUI observe a task
- **WHEN** GUI starts an import and an attached agent lists import tasks
- **THEN** the agent can query that task and observe the same terminal result as the panel

#### Scenario: Existing automation call
- **WHEN** a client invokes asset.import using only existing fields
- **THEN** its asynchronous completion and result fields remain supported

#### Scenario: Invalid request
- **WHEN** stale generation, invalid settings, incompatible type options or read-only output is submitted through either entry point
- **THEN** shared validation rejects it without publishing files

### Requirement: Import lifecycle and completion
Accepted publication SHALL remain non-cancellable and independent of panel visibility. Pending imports SHALL block root transitions and drain before their provider is destroyed. Successful publications SHALL refresh discoverable content and notify Editor to refresh its browser without replacing open drafts or adding scene history. A post-publication index failure SHALL identify committed output explicitly.

#### Scenario: Close panel during import
- **WHEN** the panel closes while import is running
- **THEN** the import continues and its result remains queryable

#### Scenario: Root transition and shutdown
- **WHEN** a root transition is requested during import
- **THEN** it reports busy; normal shutdown instead drains accepted imports before releasing IO

#### Scenario: Automation disabled
- **WHEN** automation adapters are disabled but assets and Editor are available
- **THEN** GUI import remains usable through the shared provider

### Requirement: Typed import lifecycle with compatible status projection

Import tasks and import drafts SHALL have distinct domain-owned typed states. Runtime and GUI decisions SHALL use those states. Existing string status tokens, reflected schema shape, field IDs, generation and busy/dirty behavior SHALL remain compatible; unknown boundary tokens SHALL be rejected.

#### Scenario: Task completion and failure
- **WHEN** an accepted import finishes or fails
- **THEN** its typed state maps to the existing completed or failed token and retains the existing result/error and non-cancellable lifetime

#### Scenario: Draft preparation and publication
- **WHEN** a draft is prepared, published, fails preparation, fails publication or is discarded
- **THEN** the existing preparing/ready/publishing/failed/discarded statuses remain observable
- **AND** publication failure restores Ready with its error and unchanged save point so editing/retry remain possible

#### Scenario: Boundary compatibility
- **WHEN** a known status is encoded, described and decoded
- **THEN** it remains a string with the existing token and schema; empty uninitialized preview snapshots remain empty
- **AND** invalid status tokens do not become valid lifecycle states

### Requirement: Shared sky conversion settings validation

AssetImport preflight SHALL consume Environment's authoritative sky bake-settings validation while retaining its HDR/EXR and asset-type restrictions. Reflected setting IDs, versions, defaults and descriptions and existing invalid_argument messages SHALL remain unchanged. GUI and automation SHALL continue to use the same import domain.

#### Scenario: Preflight and bake agree
- **WHEN** a compatible HDR/EXR sky import is checked with any supported or invalid bake setting tuple
- **THEN** its numeric admission agrees with Environment without duplicating the rules

#### Scenario: Incompatible import kind
- **WHEN** sky settings accompany an unsupported source extension or another asset type
- **THEN** import preflight rejects the request even when its numeric settings are valid

#### Scenario: Existing clients discover defaults
- **WHEN** a client reads the sky settings reflection or submits existing fields
- **THEN** the field shape, descriptions, defaults and existing completion/publication behavior remain compatible

### Requirement: Shared import input rules with workspace admission
The import workspace SHALL share dependency-library normalization, asset-output extension, source/output separation and source-identity rules with the independent publication service. It SHALL retain its existing Main, generation, Game-root, read-only, text-bound, importer and RootId admission, error codes and messages. GUI and automation SHALL continue to use the same workspace operation contracts.

#### Scenario: Invalid portable source identity
- **WHEN** an import request supplies only one of sourceRoot/sourceId, or sourceId contains a colon, backslash, leading slash or any double-dot substring
- **THEN** workspace validation rejects it with the existing invalid_arguments diagnostic and performs no writes

#### Scenario: Default library and root boundary
- **WHEN** a request omits its dependency library
- **THEN** validation uses the normalized output parent as library and still enforces the current writable Game root for both paths

#### Scenario: Workspace grouped admission remains stricter
- **WHEN** a grouped workspace request supplies an explicit library, even the output parent
- **THEN** validation rejects it with the existing grouped-import diagnostic without publication
