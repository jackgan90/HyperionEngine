# editor-content-browser Specification

## Purpose
Define persistent asset-root selection, safe content-session transitions, directory browsing and typed asset opening in the editor.
## Requirements
### Requirement: Select and restore asset roots
Editor SHALL provide File > Open... native directory selection and File > Recent with at most five unique successful roots. The selected directory itself SHALL map to `/Game`. Startup SHALL restore the last successful root unless explicit mount configuration overrides it; invalid restoration SHALL be reported without silently choosing another Game root. The menu bar SHALL begin with File rather than fixed HYPERION text.

#### Scenario: Select and restart
- **WHEN** a user selects a valid directory and restarts Editor
- **THEN** that directory is restored as `/Game`, with no previously opened scene automatically loaded

#### Scenario: Cancel or invalid recent
- **WHEN** the folder dialog is cancelled or a Recent path is unavailable
- **THEN** the current root and document remain unchanged and no failed root is added

### Requirement: Protected root transition
Editor SHALL offer save-and-continue, discard-and-continue and cancel for dirty documents, finish existing saves on the old root, and abort on save failure. Before changing `/Game` it SHALL close old documents, clear history/selection/previews, join old work and safely retire retained render resources. Successful transitions SHALL leave an empty document and preserve `/Engine`. Selecting the same canonical directory SHALL not close the document.

#### Scenario: Same package path in two roots
- **WHEN** A and B contain different assets at the same `/Game` path and the user switches A to B to A
- **THEN** each load resolves exclusively to its current root and no old completion or cache supplies another root's assets

#### Scenario: Save before switching
- **WHEN** the user saves and continues
- **THEN** the old document is saved under the old mapping before transition, and save failure preserves the old workspace

### Requirement: Directory tree and file grid
Content Browser SHALL show a left directory tree rooted at `/Game` and a resizable right grid of the selected directory's immediate subdirectories and `.hasset` files. Empty directories SHALL remain visible. Folder double-click SHALL navigate and synchronize the tree. Refresh and successful saves SHALL refresh listings without per-frame recursive scanning.

#### Scenario: Browse and filter
- **WHEN** a folder contains subfolders, `.hasset`, JSON and shader files
- **THEN** its right pane shows folder icons and `.hasset` file icons only, with folders first and names sorted
- **AND** hovering a folder highlights its tile without displaying an additional text tooltip
- **AND** tile names are horizontally centered beneath their icons, including each wrapped line of a long name

### Requirement: Internal asset visibility
Editor SHALL show all native project assets by default without an internal-assets toggle. Git, source caches and publication temporaries SHALL be excluded from browsing and scene discovery. Asset classification SHALL continue to use stored TypeId; unsupported editors SHALL report controlled messages without disturbing the scene.

#### Scenario: Browse native dependencies
- **WHEN** a migrated project contains model, material, texture, sky and scene hassets
- **THEN** all files are browsable without an opt-in visibility setting and Git/cache folders remain excluded

### Requirement: User-facing content paths
All editor asset-path presentation, including status/loading/saved messages, error dialogs, Open Scene, Save Scene As, tooltips and Details asset references, SHALL display paths relative to the asset root without the `/Game/` prefix. Content Browser SHALL label its root `All` and display child breadcrumbs under `All`. Editable asset paths SHALL resolve relative input against the active asset root, including after clearing a field. Asset resolution, widget identity and persisted references SHALL retain their internal package paths; native absolute paths and other virtual mounts SHALL retain their meaning.

#### Scenario: Display and open a scene
- **WHEN** the user selects or enters `Scenes/Sponza.hasset` in Open Scene
- **THEN** the editor opens `/Game/Scenes/Sponza.hasset` while the dialog displays the relative path

#### Scenario: Display status and edit references
- **WHEN** a scene is loaded or saved, an asset error is shown, or an asset reference is inspected and edited
- **THEN** Game paths appear without the virtual root prefix, and committed relative path edits still reference the corresponding `/Game` asset

### Requirement: Typed asset opening
Double-click SHALL classify a native asset by its stored TypeId. Scenes SHALL use the existing protected document-opening path; other valid types SHALL show an unsupported-type dialog, while unreadable or corrupt assets SHALL show a distinct error. Unsupported assets SHALL not close the current scene.

#### Scenario: Unsupported and corrupt files
- **WHEN** the user double-clicks a texture or malformed hasset
- **THEN** the appropriate unsupported-type or read/format message is shown and the existing document is retained
