## ADDED Requirements

### Requirement: Typed private capture exercise identity
Editor SHALL represent each accepted capture exercise with a typed identity and no request with explicit absence. One Editor-owned mapping SHALL define the `toggle`, `capture`, `unavailable` and `hud` CLI/log names. Native control flow SHALL use typed identities.

#### Scenario: Default and selected modes
- **WHEN** options omit the exercise or specify one of its four accepted values
- **THEN** omission remains inactive and each supplied value selects only its existing exercise
- **AND** the existing acceptance activation and GUI persistence policy remain unchanged

### Requirement: Preserve capture exercise command-line behavior
The `--exercise-capture` spelling, exact accepted tokens, missing/invalid-value errors, duplicate-argument ordering and isolated preference requirement SHALL remain unchanged. Production builds with BUILD_TESTING=OFF SHALL reject exercise arguments at the same boundary before value validation.

#### Scenario: Invalid values and isolated preferences
- **WHEN** an empty, unknown, case-changed or whitespace-bearing value is supplied, or a known mode uses the default preference path
- **THEN** parsing reports the existing corresponding error in the existing validation order

#### Scenario: Production rejection
- **WHEN** an exercise argument is supplied to a BUILD_TESTING=OFF Editor, with or without a valid value
- **THEN** it reports the existing acceptance-unavailable error without starting an exercise

### Requirement: Preserve capture exercise execution
The existing capture exercise input steps, frame/readiness gates, screenshot timing, timeout participation, preference persistence, success messages and window-close behavior SHALL remain unchanged. Plugin selection, RenderDoc availability handling and shared GUI/automation domain services SHALL remain unchanged.

#### Scenario: Capture preference and HUD exercises
- **WHEN** toggle or hud exercises run with the existing provider configuration
- **THEN** they perform the same GUI actions, persist the same preferences and emit the same completion markers

#### Scenario: Capture and unavailable providers
- **WHEN** capture is requested with available RenderDoc, or unavailable is requested without an available provider
- **THEN** the existing capture/replay verification or disabled-button verification follows the same path and result
