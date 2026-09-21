## MODIFIED Requirements

### Requirement: Open mounted scenes
The editor SHALL let the user choose File > Open Scene and select a native scene discovered recursively under the active Game asset root by stored asset type, independently of fixed names, directory names or mandatory Catalog entries. It SHALL display loading status and recoverable errors. Root changes and explicit refresh SHALL rebuild the list; same-named scenes SHALL be distinguished by package path.

#### Scenario: Open Sponza
- **WHEN** the user selects Sponza from the current root and confirms opening
- **THEN** its scene loads asynchronously, nodes appear in the Outliner and its rendered image appears inside the viewport

#### Scenario: Failed scene then valid scene
- **WHEN** opening a scene fails and the user subsequently opens a valid scene
- **THEN** the error is visible, the editor stays usable and the valid scene can load

#### Scenario: Uncataloged scene
- **WHEN** a valid scene hasset is added to any browsable directory under the active root and the list is refreshed
- **THEN** it appears in Open Scene without requiring a catalog update
