## ADDED Requirements

### Requirement: Independent native asset workspace
The Editor SHALL open non-scene hasset documents in one independent native window with multiple asset tabs, an asset preview and asset properties, while keeping the main scene viewport and scene Details available.

#### Scenario: Open assets while editing a scene
- **WHEN** the user double-clicks Texture, Model, Material or Sky assets in Content Browser
- **THEN** the assets open in the separate asset window and the scene remains visible with its selection and unsaved edits intact
- **AND** reopening an existing asset activates its existing tab and restores the asset window

#### Scenario: Activate the scene while an asset window overlaps it
- **WHEN** the user clicks the scene viewport or opens another asset through the main Content Browser
- **THEN** the asset window remains above its native owner and the main window remains interactive
- **AND** scene input retains focus when the scene is clicked; the asset window is not globally always-on-top

#### Scenario: Double-click an unselected asset while the main window is inactive
- **WHEN** the user double-clicks an unselected hasset in Content Browser, including while the asset window has focus
- **THEN** the activation click is delivered to the main window and that first double-click opens the asset
- **AND** a single click still only selects the asset, and subsequent unselected assets require no preliminary selection or focus click

### Requirement: Window-local input and history
Each window SHALL route navigation, save, undo/redo and editing input only to its own document context, including while another window contains unsaved edits.

#### Scenario: Alternate scene and asset commands
- **WHEN** the user edits an asset and then performs scene undo/redo or camera navigation in the main window
- **THEN** the asset draft/history/camera remain unchanged
- **AND** asset-window commands likewise leave scene authored state/history/camera unchanged

#### Scenario: Scene save is unavailable or fails
- **WHEN** main-window Ctrl+S occurs while a scene is not ready or a document modal is active
- **THEN** no save is admitted
- **AND** a save-time exception is reported within the Editor without exiting or discarding any document

### Requirement: Independent presentation and scaling
The Editor SHALL render and resize each visible native window independently, respecting its framebuffer density and the shared application scale.

#### Scenario: Minimize or resize one window
- **WHEN** the asset window is minimized or either window is resized
- **THEN** the main viewport remains interactive and each visible window continues rendering
- **AND** background asset loading and admitted saves continue

#### Scenario: Minimize the main application window
- **WHEN** the main window is minimized and then restored
- **THEN** its owned asset window follows the native window-group visibility behavior, with both documents preserved
- **AND** rendering pauses while the owner is minimized, while loading and admitted saves continue

### Requirement: Protected asset-window lifecycle
Closing the asset window SHALL affect only asset documents, offer save/discard/cancel for unsaved changes, preserve documents on save failure and release resources safely after admitted work. Application exit and content-root changes SHALL protect both scene and asset documents.

#### Scenario: Cancel or accept asset-window closure
- **WHEN** the user closes the asset window with unsaved documents
- **THEN** Cancel preserves the window and drafts, Save closes only after successful saves, and Discard closes without persisting drafts
- **AND** the main scene and application remain open with their authored state intact

#### Scenario: Reopen after closure or switch content roots
- **WHEN** another asset is opened after closing the asset window, or a protected content-root change completes
- **THEN** the asset host can be recreated without stale document, GUI or GPU references

### Requirement: Shared saved-asset refresh
Successful asset saves SHALL refresh main-scene references and open asset previews while retaining unsaved scene edits and independent histories.

#### Scenario: Save while observing both windows
- **WHEN** a material or texture referenced by the visible scene is saved in the asset window
- **THEN** the scene uses the saved asset after refresh without reload, selection loss or history reset
