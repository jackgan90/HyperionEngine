## MODIFIED Requirements

### Requirement: Complete scene settings UI
Editor SHALL expose default camera, main directional light, environment light and fixed-KeepWorld drag-and-drop reparenting through the shared document. Details SHALL NOT display a Hierarchy section or reparent-mode selectors. Persistent edits SHALL support history and save/reopen; browsing camera changes SHALL remain transient. Existing single-node Automation reparent modes SHALL remain compatible.

#### Scenario: Author environment selection
- **WHEN** the environment selection is changed, undone, redone and saved
- **THEN** GUI and Automation observe the same settings and reopening restores the saved selection

#### Scenario: Reparent through Outliner
- **WHEN** selected Outliner rows are dragged onto an Outliner parent
- **THEN** Editor performs one fixed-KeepWorld batch transaction without asking for a transform mode
