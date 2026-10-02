# native-asset-editors Specification

## Purpose
Define typed native asset inspection and editing with isolated previews, persisted properties, independent document history and protected saves and closure.
## Requirements
### Requirement: Independent asset documents
Editor SHALL open Model, Texture, Material and Sky hassets in multiple document tabs with independent draft, save state and undo/redo, alongside the existing single scene document. Reopening the same native identity SHALL focus its existing document. Commands SHALL target the active document. All dirty documents SHALL participate in close, exit and root-transition protection. Unknown, corrupt or obsolete types SHALL receive diagnostics without data writes.

#### Scenario: Multiple documents and history
- **WHEN** a texture and material are edited in separate tabs and the material receives Undo
- **THEN** only the material draft changes and neither the texture nor scene history is altered

#### Scenario: Close dirty workspace
- **WHEN** a root switch or exit is requested with dirty documents or admitted saves
- **THEN** save/discard/cancel applies to those documents and no pending work crosses into the new content session

### Requirement: Asset inspection and typed editing
Each supported editor SHALL provide a primary display and property panel that visibly distinguishes editable persisted fields, read-only information and transient preview controls. Common information SHALL include identity, type, schema, path, revision, dependencies and load/save errors. Type adapters SHALL validate complete candidate values before committing. Existing references SHALL be chosen by native identity with type/dimension/graph checks; failed choices SHALL retain the previous value.

#### Scenario: Reference replacement
- **WHEN** a material texture or existing model material slot is changed to another valid existing asset
- **THEN** preview updates, one history transaction is recorded and save/reload preserves the chosen native reference

### Requirement: Texture editor
Texture preview SHALL support stored 2D images and cube faces, selected mips, channel toggles, alpha background, pan/zoom, pixel values and floating-point exposure. It SHALL display actual dimensions, format, encoding, mip and byte statistics. Name SHALL be editable. RGBA8 Texture2D encoding changes SHALL retain mip0 bytes and rebuild the complete lower mip chain atomically as one undoable edit. Cube encoding and floating-point Linear encoding SHALL remain read-only.

#### Scenario: Encoding and exact undo
- **WHEN** a multilevel RGBA8 texture changes encoding and is undone, redone, saved and reopened
- **THEN** each restored encoding has its corresponding exact mip payload and the preview uses the correct display conversion

#### Scenario: Close while encoding is being rebuilt
- **WHEN** a texture encoding edit is accepted but its asynchronous mip rebuild has not yet been applied
- **THEN** the workspace treats it as unsaved, tab/window/exit/root closure requires an explicit choice, and save actions wait until the edit is applied
- **AND** Discard explicitly cancels the pending edit; Undo/Redo resumes after the atomic edit completes

### Requirement: Model and sky editors
Model preview SHALL render native geometry/materials with independent reusable camera navigation and framing. It SHALL expose hierarchy, stable subresource IDs, geometry/attribute counts, bounds and material slots; names, local node PRS, existing primitive-slot assignment and existing slot references SHALL be editable. Sky preview SHALL show sky and reference-surface lighting from its native products; name SHALL be editable while bake references, SH coefficients and convention remain read-only. Preview lighting/camera/exposure SHALL not modify assets.

#### Scenario: Model node edit
- **WHEN** a node transform or slot is edited, saved and reopened
- **THEN** the model preview and stored value agree without changing geometry topology or stable subresource IDs

#### Scenario: Stable preview during continuous edits
- **WHEN** model transform dragging repeatedly prepares and publishes preview updates
- **THEN** readiness text remains available in a fixed-height status area without changing the preview canvas bounds or aspect ratio
- **AND** the drag remains one undoable edit, and Undo/Redo preserves the preview layout

#### Scenario: Sky preview controls
- **WHEN** sky preview exposure or camera orientation changes
- **THEN** its appearance changes without dirtying the sky or scene document

### Requirement: Material editor
Material preview SHALL render the asset's actual shader/pass data on selectable engine sphere, plane or cube geometry. Property editing SHALL support existing permitted numeric scalar/vector/color values, simple fixed-layout leaves, native texture references, samplers and UV selection, preserving parameter declarations. Default versus explicit asset values SHALL be distinguishable and Reset SHALL remove the explicit override. Engine-provided/locked parameters, complex layouts and coordinated shader/pass state changes SHALL be read-only. Shader files SHALL remain excluded from Content Browser and have no editor.

#### Scenario: Custom shader parameter
- **WHEN** a material with a custom non-PBR scalar/vector parameter is opened and edited
- **THEN** its parameter is typed correctly, the actual shader preview updates and save/undo/redo preserve it

### Requirement: Snapshot save and bounded lifecycle
Saving SHALL preserve native identity, reject a changed loaded ID/revision baseline, use atomic native persistence and record the submitted document state. Edits made during a save SHALL remain dirty after that save. Draft previews SHALL use owned immutable snapshots without temporary native files; hidden tabs SHALL skip preview rendering and all async/GPU ownership SHALL follow existing lifecycle contracts.

#### Scenario: Editing while saving
- **WHEN** state A is submitted for save and the user creates state B before completion
- **THEN** disk and saved baseline contain A while the document remains dirty with B

#### Scenario: Closing a rendered asset tab
- **WHEN** the user closes a clean asset tab or discards a dirty tab after its preview was submitted in the current GUI frame
- **THEN** only that document closes, its preview resources survive the submitted frame and other documents and the Editor remain usable

#### Scenario: Moving panels while an asset tab is active
- **WHEN** multiple asset tabs are open and the user holds the left mouse button while moving or docking the Place Object panel
- **THEN** the panel follows the pointer continuously until release, without scene placement cancellation interrupting the window gesture

### Requirement: Canonical asset field editing policy

AssetEditing SHALL associate editable field access, validation/normalization, reference preparation and effects with the canonical reflected member identity. GUI, standalone and attached automation SHALL use that same domain policy while preserving external operation IDs, schemas and persisted fields. Reflection SHALL NOT own asset editing or preview policy. Generic write availability SHALL NOT be inferred merely from reflected readability, and dedicated primitive and texture encoding operations SHALL retain their restrictions.

#### Scenario: Same member through different callers
- **WHEN** GUI and either automation provider edit the same supported field from equivalent document states
- **THEN** normalization, validation, reference requirements, committed values and permitted operation route agree

#### Scenario: Read-only or dedicated field
- **WHEN** a readable field is read-only or requires a dedicated primitive/encoding operation
- **THEN** generic Set does not expose a new write operation or bypass the existing identity, topology, geometry or encoding restrictions

### Requirement: Asset domain owns preview change effects

Preview invalidation SHALL derive from normalized before/after values in the asset domain and SHALL NOT be supplied or overridden by adapter callers. Asset names, model-node names and primitive names alone SHALL preserve preview generation. Model-node Local changes, primitive material assignment changes, effective material/slot value changes and committed encoding/payload changes SHALL invalidate as appropriate. History SHALL retain the computed effects so Undo, Redo, Cancel and coalesced interactions apply equivalent invalidation while preserving existing generation and persistence behavior.

#### Scenario: Node rename through automation
- **WHEN** model.nodes.set changes only node names while retaining identities, topology and Local values
- **THEN** the persisted draft and history update with the same preview-generation behavior as the corresponding GUI edit, without rebuilding the preview

#### Scenario: Mixed name and transform change
- **WHEN** one node edit changes its name and Local transform
- **THEN** the domain records the existing transaction and invalidates the preview for the transform effect, including Undo/Redo

#### Scenario: Primitive metadata and material
- **WHEN** an existing primitive is renamed or assigned another valid existing material slot
- **THEN** rename alone preserves preview generation while material change invalidates, independently of caller

### Requirement: Prepared asset policies preserve bounded asynchronous completion

Prepared policy values and reference requirements SHALL be owned. Completion SHALL retain current document identity/generation checks and required reference type/dimension/graph validation before applying an edit. Closed/replaced/stale documents and failed requirements SHALL leave authoritative values and history unchanged. Drain and destruction SHALL join/release accepted work without committing. Texture root edits SHALL preserve exact encoding/mip history and shared unchanged bulk storage.

#### Scenario: Prepared reference becomes stale
- **WHEN** reference preparation completes after document generation or identity changes
- **THEN** the edit is rejected without changing the new document or history and admission is released

#### Scenario: Drain pending encoding or references
- **WHEN** a pending edit is drained during closure
- **THEN** work is joined and ownership released without applying the candidate or dirtying another document
