# scene-management Specification

## Purpose
Define renderer-independent Main scene ownership, stable model instance handles, shared immutable assets and ordered rendering synchronization.

## Requirements
### Requirement: Renderer-independent logical scene
The Scene module SHALL own flat logical model instances independently of Renderer and RHI. Handles SHALL include scene identity, slot and generation. Main mutations SHALL support add, remove, asset attachment, transform, visibility and material override with monotonic revisions and owned incremental snapshots.

#### Scenario: Reuse and foreign handles
- **WHEN** an old or foreign handle is used after slot reuse
- **THEN** the current instance remains unchanged

#### Scenario: Shared asset independent state
- **WHEN** two instances reference one immutable asset and one is moved or hidden
- **THEN** the other retains its state and both share immutable asset data

### Requirement: Ordered rendering attachment
A Main rendering bridge SHALL publish complete instance state through the existing ordered Render client. Model member changes and removal SHALL be frame-atomic. Render SHALL NOT borrow Main mutable objects. Accepted removals SHALL progress without presentation and preserve existing frame leases.

#### Scenario: Initial transform and removal
- **WHEN** a transformed model is attached and subsequently removed while a snapshot exists
- **THEN** no identity-transform frame is exposed and the old snapshot remains valid after removal

#### Scenario: Late asset result
- **WHEN** loading finishes after the logical instance was removed
- **THEN** it cannot attach an asset or register primitives for a reused slot

#### Scenario: Minimized scene
- **WHEN** scene mutations occur while presentation is paused
- **THEN** the bridge and Render control queue continue applying changes and cleanup

### Requirement: CPU material selection and overrides
Logical Scene models SHALL support CPU material instance references, model-default and asset-section-specific replacements, and typed model/section overrides without RHI dependencies. Resolution SHALL select section replacement before model replacement before imported default, then apply model and section overrides in that order. Existing base color, metallic and roughness override entrypoints SHALL remain compatible; an explicit generic override for the same parameter SHALL take precedence. Unknown sections or incompatible parameters SHALL report errors rather than be silently ignored.

#### Scenario: Override one section
- **WHEN** a model selects an independent material for one asset section
- **THEN** all occurrences of that section use the selected material and other sections retain their chosen defaults without reuploading geometry

#### Scenario: Selection precedes loading
- **WHEN** a material selection is submitted before the asset finishes loading and references a nonexistent section
- **THEN** the bridge publishes an actionable error after validation while the model remains removable and other models remain usable

### Requirement: Shared material revision synchronization
The Main bridge SHALL track referenced material revisions independently of Scene.Update calls, freeze each revision once per Flush and publish affected related primitives in an atomic batch. Scene change data crossing into Render SHALL contain immutable snapshots rather than mutable material references. Cloning a logical model SHALL preserve explicit material-sharing semantics.

#### Scenario: Shared material changes without model mutation
- **WHEN** two models reference one edited material instance and neither model calls Scene.Update
- **THEN** the next bridge Flush publishes the new material revision to both models without exposing mixed revisions in one frame

#### Scenario: Model copy
- **WHEN** a model is copied with the same explicit material instance reference
- **THEN** the copy shares subsequent material edits while independently copied material instances and local overrides remain isolated
