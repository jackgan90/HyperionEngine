# selection-outlines Specification

## Purpose
Define reusable silhouette outlines for selected geometry, with editor activation, depth-independent coverage, configurable union or per-object overlap, and safe frame and material ownership.
## Requirements
### Requirement: Exterior outlines independent of scene occlusion
The renderer SHALL derive an orange exterior outline from selected geometry coverage without testing against scene depth. Internal mesh edges, surface normals and material boundaries MUST NOT create outlines. All sections belonging to one requested object SHALL share its silhouette.

#### Scenario: Occluded multi-section object
- **WHEN** a selected model with several sections is partially or completely behind an unselected occluder
- **THEN** its projected silhouette has an orange exterior outline, internal section seams are absent, and its interior scene color is unchanged

### Requirement: Configurable overlap policy
Outline requests SHALL support multiple object groups and both Union and PerObject modes. Union SHALL be the default. Union SHALL extract edges after combining coverage; PerObject SHALL combine edges extracted independently for each object.

#### Scenario: Overlapping selected objects
- **WHEN** two selected objects overlap or one projection contains the other
- **THEN** Union draws only their combined silhouette, while PerObject retains both independent outlines including covered parts

#### Scenario: No selection
- **WHEN** the request is empty or the feature is absent
- **THEN** no outline passes or exclusive outline textures are produced

### Requirement: Owned targets and frame consistency
Main SHALL resolve scene selection to generation-safe primitive groups and an exact scene publication. Render SHALL consume owned immutable requests, respect enabled and section visibility, and reject mismatched publication without accessing Main-owned mutable state. Deferred passes and GPU resources SHALL retain their inputs until safe retirement.

#### Scenario: Scene replacement and deletion
- **WHEN** a selected object is deleted, replaced, restored with a new generation, or belongs to a previous scene attachment
- **THEN** an old request never highlights a different object and the next valid frame reflects the current selection

### Requirement: Material coverage and display composition
Standard PBR masked geometry SHALL reuse its effective alpha texture, UV and cutoff inputs. Blended geometry SHALL use geometric coverage. Custom materials SHALL provide a compatible SilhouetteMask pass or report unsupported coverage. Composition SHALL follow tonemapping, use correct color encoding, preserve output alpha and remain below GUI overlays.

#### Scenario: Masked surface
- **WHEN** a selected standard PBR surface has transparent cutouts and material overrides
- **THEN** the mask follows its effective coverage rather than the full underlying triangles

#### Scenario: Custom reflected inputs
- **WHEN** a custom SilhouetteMask pass uses reflected inputs with material or object overrides, including overrides used only by the ordinary color pass
- **THEN** the auxiliary material preserves the complete logical source interface, applies the effective clipping inputs, and produces binary coverage without rejecting inactive overrides or aborting the frame

#### Scenario: Exposure and resize
- **WHEN** exposure, viewport dimensions or depth convention changes
- **THEN** orange display color and pixel width remain stable, targets match the viewport, and no stale outline is retained

### Requirement: Editor activation and comparison
Editor SHALL activate the reusable Renderer feature for existing Outliner and viewport selection and expose an immediate Union / PerObject viewport option. Selection and mode changes MUST NOT dirty the document. Viewer SHALL not activate outlines by default. A multi-target comparison exercise SHALL demonstrate both modes without requiring multi-selection UI.

#### Scenario: Switching modes
- **WHEN** the user changes the viewport outline mode
- **THEN** the next frame uses that mode without restarting plugins, changing selection, or creating an undo transaction

#### Scenario: Multiple targets without multi-selection UI
- **WHEN** the comparison exercise supplies multiple selected scene objects
- **THEN** both policies run through the production feature and generate distinguishable comparison images
