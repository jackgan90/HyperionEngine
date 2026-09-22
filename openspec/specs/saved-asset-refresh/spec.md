# saved-asset-refresh Specification

## Purpose
Define publication of saved native asset dependencies to live scene and preview consumers while preserving unsaved authored state, history and resource lifetimes.
## Requirements
### Requirement: Save-triggered dependency publication
Unsaved asset edits SHALL affect only their document preview. Successful native saves SHALL refresh affected open scene and asset-preview consumers through direct and transitive dependencies. Refresh SHALL retain dirty drafts, scene authored edits, handles, selection, camera, material overrides and history. Refresh SHALL not become an authored edit or dirty an unaffected document. Stale completions SHALL be rejected; save and refresh errors SHALL be distinguishable.

#### Scenario: Shared texture save
- **WHEN** a texture used by two open materials and the current scene is saved
- **THEN** all affected previews and scene instances use its saved content while unrelated assets and unsaved authoring state remain intact

#### Scenario: Create consumers during refresh
- **WHEN** a model is added or duplicated after dependency refresh captures its consumers but before publication
- **THEN** the new consumers also receive the saved resources, retaining their authored names, transforms and material overrides

#### Scenario: Undo after save
- **WHEN** an asset is saved and then locally undone without another save
- **THEN** only its draft preview reverts and other consumers continue using the published saved content

### Requirement: Resource-safe scene refresh
Scene dependency refresh SHALL update resolved data on existing scene nodes without reloading the scene document. It SHALL retain prior valid rendering until replacement is ready and preserve normal GPU retirement. Undo/redo SHALL restore authored historical values against current published dependencies instead of resurrecting stale resource snapshots. Material-only refresh SHALL not require geometry reimport.

#### Scenario: Scene history survives dependency changes
- **WHEN** a scene transform or material override is edited, a shared material is saved, and the scene edit is undone/redone
- **THEN** the authored historical values are restored while the shared asset remains at its latest saved content
