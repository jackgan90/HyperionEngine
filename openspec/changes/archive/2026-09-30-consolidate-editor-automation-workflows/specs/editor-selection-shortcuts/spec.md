## ADDED Requirements

### Requirement: Shared scene shortcut admission
Editor SHALL evaluate scene shortcut input ownership through one current-frame policy with explicit command-specific eligibility. Delete, selection, clipboard, framing and history SHALL respect applicable window focus, text ownership, popup/modal and active gesture constraints. Delete SHALL require Viewport or Outliner focus and an idle scene document. Completing an inspector transaction for an eligible command SHALL preserve existing history grouping.

#### Scenario: Delete outside scene focus
- **WHEN** a scene selection exists and Delete is pressed while Content Browser, Log, text input or popup UI owns input
- **THEN** the selected scene nodes, authored revision and history remain unchanged

#### Scenario: Input ownership changes in the current batch
- **WHEN** focus, text ownership or a conflicting gesture changes in the same input batch as a shortcut
- **THEN** routing uses the effective current ownership and cannot execute a scene mutation using stale prior-frame admission

#### Scenario: Eligible command remains available
- **WHEN** the Viewport or Outliner has focus and an applicable shortcut is admitted without competing ownership
- **THEN** it uses the shared document operation and preserves established selection and history semantics
