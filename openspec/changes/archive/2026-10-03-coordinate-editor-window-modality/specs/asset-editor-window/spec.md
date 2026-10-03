## ADDED Requirements

### Requirement: Main modal flow takes priority over auxiliary windows
The Editor SHALL register its asset window with the shared Platform window group and publish existing main-modal state independently of asset-window existence. Main confirmation dialogs SHALL appear above registered auxiliary windows and receive exclusive group input without changing document protection or automation contracts.

#### Scenario: Close a dirty scene with an overlapping asset window
- **WHEN** the scene has unsaved changes, an asset window is open and the native Main close button is pressed
- **THEN** the save/discard/cancel confirmation is visible above the asset window and the asset title bar and client area cannot steal input
- **AND** Cancel preserves drafts, Save persists before exit, and Discard exits without persisting the scene draft

#### Scenario: Add another auxiliary host
- **WHEN** another Editor auxiliary window uses the same scoped registration contract
- **THEN** existing modal coordination applies without per-window owner reversal or new modal-type branches
