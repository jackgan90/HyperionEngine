# typed-import-draft-history Specification

## Purpose
Define typed import draft history actions and their shared execution while preserving existing request formats, validation order and history transactions.
## Requirements
### Requirement: Import draft history has typed domain actions
AssetImport SHALL own explicit Reset, Undo and Redo action identities. GUI and native history consumers SHALL use the typed domain entry point. The existing string request SHALL be parsed once at the AssetImport boundary and delegate to the same history implementation.

#### Scenario: Native and protocol actions share history behavior
- **WHEN** a native enum action or its existing protocol token requests undo, redo or reset
- **THEN** the same validation, history transaction, generation, dirty state and undo/redo availability apply without publishing files

#### Scenario: Reset remains undoable
- **WHEN** a modified draft is reset and the next action is undo
- **THEN** the source properties are first restored and the previous edits can subsequently be restored through the same bounded history

### Requirement: Existing history request contracts remain compatible
The reflected FImportDraftHistory request SHALL retain its type ID, members, version, string action shape and empty default. Existing known and unknown strings SHALL retain archive and wire round-trip behavior. Operation discovery, schemas and examples SHALL remain unchanged.

#### Scenario: Unknown input reaches domain validation
- **WHEN** the action is omitted, empty, unknown or differs in case
- **THEN** string decoding preserves the request and the shared domain service applies the existing error classification, diagnostic and validation order

#### Scenario: Non-string input remains rejected
- **WHEN** an action is supplied with a non-string JSON value
- **THEN** the existing reflection boundary rejects it without a history mutation

### Requirement: History validation and mutation order is preserved
History operations SHALL retain Main-domain, draft existence, busy and generation checks before action availability validation. Invalid or unavailable actions SHALL preserve the prior exception type, error code and diagnostic, and SHALL NOT mutate the draft, task list or published content.

#### Scenario: Earlier failures take precedence
- **WHEN** an invalid action is combined with a missing draft, busy draft or stale generation
- **THEN** the original earlier failure is reported and draft history remains unchanged

#### Scenario: History boundary rejects unavailable actions
- **WHEN** undo or redo is unavailable, or an enum value is invalid
- **THEN** the request fails with the existing invalid_arguments result and leaves the draft snapshot unchanged
