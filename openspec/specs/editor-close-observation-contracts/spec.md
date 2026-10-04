# editor-close-observation-contracts Specification

## Purpose
Define typed observations of Editor close transitions while preserving public status projections and existing lifecycle behavior.
## Requirements
### Requirement: Close semantics are asserted through the transition phase

Editor native transition tests and content-close GUI acceptance SHALL use the existing typed close phase for semantic decisions. They SHALL distinguish waiting for a decision, waiting for a save path, active saving, failure, completed close readiness, reconfirmation and absence of a close request while retaining existing intent, error and side-effect assertions.

#### Scenario: Textually identical close states
- **WHEN** a dirty close awaits a decision, a close waits for a path, or a ready close is challenged by new dirty state
- **THEN** tests assert AwaitingDecision, AwaitingSavePath or Reconfirming respectively rather than accepting any phase with the same status text

#### Scenario: Save failure and cancellation
- **WHEN** a scene save error is recorded and dirty continuation advances, or the GUI cancels a failed close
- **THEN** tests verify the exact Saving-to-Failed or Failed-to-Idle transition alongside retained dirty work and pending intent semantics

### Requirement: Close status projection remains compatible

The existing public close status text SHALL remain a projection of the transition owner with unchanged tokens and grouping. Focused compatibility coverage SHALL exercise reachable phases through domain operations, while semantic tests SHALL not depend on those text labels. Production state, automation operations and schemas SHALL remain unchanged.

#### Scenario: Reachable phase projections
- **WHEN** close transitions reach Idle, AwaitingDecision, AwaitingSavePath, Saving, Failed, Ready and Reconfirming
- **THEN** their status texts remain idle, idle, idle, saving, failed, closing and closing respectively
