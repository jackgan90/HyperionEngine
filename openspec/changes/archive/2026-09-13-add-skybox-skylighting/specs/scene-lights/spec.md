## ADDED Requirements

### Requirement: Persistent environment source selection
Environment lights SHALL select either ConstantColor or SkyAsset, with a native sky reference, finite yaw, shared radiance intensity and independently controlled background visibility. Sky orientation SHALL use explicit yaw, independent of node transform. SkyAsset mode SHALL replace constant ambient. Existing environment records SHALL retain ConstantColor behavior. Snapshot and Save As SHALL preserve and rebase requested references, including pending or failed choices.

#### Scenario: Save a sky selection
- **WHEN** a sky reference, yaw, intensity and visibility are edited and saved to another directory
- **THEN** loading the saved scene restores the same requested environment and correctly resolves its pinned dependencies

### Requirement: Generation-safe environment publication
Environment loading SHALL be asynchronous and selection-generation-safe. Complete sky resources SHALL publish together; initial readiness SHALL include environment readiness. Failed or stale loads SHALL NOT overwrite newer choices, mix visual/lighting generations or implicitly enable fixed ambient. Retained complete skies MAY remain visible during replacement while requested state and failures remain observable.

#### Scenario: Rapid selection and failure
- **WHEN** sky A is replaced by B and then a missing asset before B completes
- **THEN** stale completions do not become authoritative, the failure is reported, and saving never substitutes the previously rendered asset for the requested one
