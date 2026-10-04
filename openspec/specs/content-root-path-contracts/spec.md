# content-root-path-contracts Specification

## Purpose
Define Content-owned logical root identity and shared missing-root validation while preserving IO safety, caller error contracts and Editor path presentation.
## Requirements
### Requirement: Content owns logical root identity
Content SHALL define stable Game and Engine root identities and shared lexical path membership queries. Membership SHALL include the exact root and paths beginning with that root followed by `/`, using case-sensitive comparison. Membership SHALL NOT normalize input or replace IO validation.

#### Scenario: Root boundary is distinguished
- **WHEN** consumers classify `/Game`, `/Game/Asset.hasset`, `/Gameplay/Asset.hasset`, `/game/Asset.hasset` and `/Engine/Asset.hasset`
- **THEN** only the first two are classified as Game content, while Engine classification uses its own exact root boundary

#### Scenario: Invalid paths still reach their original validation
- **WHEN** a directory request contains parent traversal, backslashes, an unsupported root or a physical path
- **THEN** the original directory validation rejects it with the existing error classification and precedence

### Requirement: Missing Game root validation is shared without changing callers
Directory queries, import output validation and standalone asset opening SHALL use Content-owned missing-root validation. Each caller SHALL preserve its prior exception type, error code, diagnostic text, check order and absence of side effects. Engine access and asset providers without a root service SHALL preserve their existing behavior.

#### Scenario: Game root is unset
- **WHEN** a valid-generation Game directory query, import output validation or standalone Game asset open is requested without a selected Game directory
- **THEN** the operation rejects with its existing `root_unset` result before creating documents or publishing data

#### Scenario: Earlier errors win
- **WHEN** a stale request, invalid directory page size, invalid directory syntax or parent traversal coincides with an unset Game root
- **THEN** each entry point reports the same earlier error as before the refactor

### Requirement: Editor consumes root contracts while keeping path presentation separate
Editor SHALL reuse Content root identities for root defaults, comparisons and path composition and SHALL reuse FGuiPathDisplay for relative Game path display. Stored paths and request defaults SHALL remain unchanged; Gui SHALL remain independent of Content.

#### Scenario: Browser displays Game paths
- **WHEN** the root itself, a Game descendant, an Engine path or a similar prefix is displayed
- **THEN** the existing relative Game presentation is retained and unrelated paths remain intact

#### Scenario: Operations remain discoverable and compatible
- **WHEN** existing content root, directory, asset open and import validation operations are described and invoked
- **THEN** their operation IDs, schemas, examples, defaults and validation outcomes remain unchanged
