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
