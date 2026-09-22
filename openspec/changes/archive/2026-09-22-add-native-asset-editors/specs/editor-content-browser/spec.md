## MODIFIED Requirements

### Requirement: Typed asset opening
Double-click SHALL classify a native asset by its stored TypeId. Scenes SHALL use the existing protected document-opening path; Model, Texture, Material and Sky SHALL open their asset editor documents without closing the current scene. Unreadable/corrupt, unknown and retired assets SHALL show distinct diagnostic information without modifying current documents. Shader text files SHALL remain excluded.

#### Scenario: Supported and corrupt files
- **WHEN** the user double-clicks each supported native type or a malformed hasset
- **THEN** the supported type opens its editor and the malformed file shows an error while the existing scene is retained

### Requirement: Protected root transition
Editor SHALL offer save-and-continue, discard-and-continue and cancel for all dirty scene and asset documents, finish existing saves on the old root, and abort on save failure. Before changing `/Game` it SHALL close old documents, clear history/selection/previews, join old work and safely retire retained render resources. Successful transitions SHALL leave an empty document and preserve `/Engine`. Selecting the same canonical directory SHALL not close any document.

#### Scenario: Same package path in two roots
- **WHEN** A and B contain different assets at the same `/Game` path and the user switches A to B to A
- **THEN** each load resolves exclusively to its current root and no old completion or cache supplies another root's assets

#### Scenario: Save before switching
- **WHEN** the user saves and continues with multiple dirty documents
- **THEN** all old documents are saved under the old mapping before transition, and any save failure preserves the old workspace
