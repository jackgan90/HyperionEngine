## ADDED Requirements

### Requirement: Independent window lifetime
Destroying a window SHALL release only its own native window and balanced subsystem references, leaving surviving windows operational.

#### Scenario: Destroy a peer window
- **WHEN** two windows exist and either window is destroyed
- **THEN** the surviving window can query its size, resize and pump events, and a new window can subsequently be created
