## MODIFIED Requirements

### Requirement: Select and restore asset roots
Editor SHALL provide File > Open... native directory selection and File > Recent with at most five unique successful roots. The selected directory itself SHALL map to `/Game`. Startup SHALL restore the last successful root unless an explicit asset directory overrides it; absent preferences SHALL leave Game unmounted and the browser empty. Invalid restoration SHALL be reported without silently choosing another Game root. Root changes SHALL use the shared content root service. The menu bar SHALL begin with File rather than fixed HYPERION text.

#### Scenario: Restart after root selection
- **WHEN** the user successfully chooses a directory and restarts Editor
- **THEN** that directory is restored as `/Game`, with no previously opened scene automatically loaded

#### Scenario: First launch
- **WHEN** no saved or explicit asset root exists
- **THEN** the Content Browser asks the user to select a directory and performs no Game asset discovery
