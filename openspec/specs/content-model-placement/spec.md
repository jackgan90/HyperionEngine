# content-model-placement Specification

## Purpose
Define native model placement from the Content Browser through shared GUI and automation services, including asynchronous preparation, source-material previews, cancellation, publication handoff, history and persistence.
## Requirements
### Requirement: Native model viewport placement
The Editor SHALL accept native model assets from the existing Content Browser asset payload and create one root scene model node with the filename stem as its display name, preserving authored internal instances and source materials.

#### Scenario: Ready model dropped in viewport
- **WHEN** a valid model has a ready placement preview and is released in the viewport
- **THEN** one node SHALL be created and selected, viewport keyboard focus SHALL transfer, and one undoable history action SHALL be recorded

#### Scenario: Incompatible asset
- **WHEN** a texture, material, sky, scene, unknown or invalid asset is dragged over the viewport
- **THEN** placement SHALL be rejected with feedback and SHALL NOT create a node or history action

### Requirement: Shared placement pipeline
Preset and native model placement SHALL share candidate preparation, viewport positioning, transient preview and node commit behavior. Existing preset IDs and generic asset property drops SHALL remain compatible.

#### Scenario: Multipart preview
- **WHEN** a model containing multiple transformed instances is previewed
- **THEN** all instances SHALL use their authored transforms relative to the same placement pivot and the preview SHALL NOT become scene content

#### Scenario: Existing presets
- **WHEN** a registered shape or light is dragged from Place Object
- **THEN** its existing positioning, preview, cancellation, selection and history behavior SHALL remain available

### Requirement: Asynchronous preparation and cancellation
The Editor SHALL prepare model resources asynchronously, permit commit only with a current ready preview, and cancel placement on release before readiness or on invalidated interaction context.

#### Scenario: Release during loading
- **WHEN** the user releases a model before preparation completes
- **THEN** no node SHALL be created now or when the request later completes

#### Scenario: Cancel or replace context
- **WHEN** Escape, lost focus, outside release, a modal/viewport change, scene replacement or content-root replacement invalidates the gesture
- **THEN** preview ownership SHALL be cleared and no stale candidate SHALL commit

#### Scenario: Leave and return
- **WHEN** the pointer leaves the viewport while the drag remains held and returns without invalidating context
- **THEN** the preview SHALL hide outside and resume inside

### Requirement: Shared references and persistence
Placed models SHALL use the native reference registration and shared document history. Cancellation SHALL NOT persist unused model registrations.

#### Scenario: Repeat undo redo and save
- **WHEN** the same model is placed twice, undone/redone and saved/reopened
- **THEN** the scene SHALL retain two independent node transforms using shared native asset data and source materials

#### Scenario: Cancelled asset save
- **WHEN** a prepared model drag is cancelled and the scene is saved
- **THEN** an otherwise unused model reference SHALL NOT appear in the saved scene

#### Scenario: Previously prepared model updated
- **WHEN** an unused prepared model is invalidated by an asset save and subsequently placed again
- **THEN** the new placement SHALL prepare the current model data rather than reusing the invalidated registration

### Requirement: Equivalent typed automation
The operation `scene.placement.place_model` SHALL expose reflected document, revision, model and world pivot inputs and return the committed scene node through the same UI-independent preparation and commit implementation used by GUI placement.

#### Scenario: Discovery and invocation
- **WHEN** a client discovers, describes and invokes model placement with a current valid request
- **THEN** preparation SHALL complete asynchronously and create/select one undoable node without writing to disk

#### Scenario: Stale invalid busy or unavailable
- **WHEN** the provider is absent, the request is stale/busy, or model validation/preparation fails
- **THEN** the operation SHALL report a controlled failure without creating a node or history action

### Requirement: Source material preview
Native model placement previews SHALL prefer each section's ready source material and SHALL use the independent shaded material while that section's render material is not ready. Previews SHALL use normal scene lighting and material pass selection without becoming persistent scene content or shadow-map casters.

#### Scenario: Material readiness transition
- **WHEN** geometry is ready while a source material is still preparing or uploading
- **THEN** that section SHALL remain visible with the shaded fallback and SHALL switch to its source material on a subsequent preview frame after readiness, without restarting the gesture

#### Scenario: Cached source material
- **WHEN** a model's source materials are already ready at preview creation
- **THEN** the first preview frame SHALL use those source materials

#### Scenario: Normal material behavior
- **WHEN** a ready model is previewed in Forward or Deferred rendering
- **THEN** its source colors, masked coverage and transparency SHALL use normal view pass selection and transparent ordering with scene objects
- **AND** removing the preview SHALL leave the scene and subsequent frames unchanged

#### Scenario: Release with pending source materials
- **WHEN** a valid preview is committed while some source materials are still preparing or uploading
- **THEN** the transition SHALL retain shaded fallback for pending sections and select ready source materials on subsequent frames until the formal model is ready
- **AND** the preview and corresponding formal primitives SHALL NOT both contribute to the same view

#### Scenario: Transparent release and publication failure
- **WHEN** a transparent model transitions from preview to the formal scene node
- **THEN** each section SHALL contribute once per view, without duplicate blending on the release frame
- **AND** a failed scene publication SHALL retire the transition through controlled error handling without throwing from the readiness query
