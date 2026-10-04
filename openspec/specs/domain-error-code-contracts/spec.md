# domain-error-code-contracts Specification

## Purpose
Define domain-owned error identities and shared automation conversion that preserve stable failure envelopes across immediate and deferred execution.
## Requirements
### Requirement: Error identities are owned by their domains
Known error identifiers SHALL be declared once by each owning domain and used as typed identities for internal throws and decisions. The common representation SHALL remain open to external codes and SHALL NOT define a closed Core enumeration of domain errors.

#### Scenario: Known internal and unknown external errors
- **WHEN** a known domain failure or an unrecognized external/provider code crosses a boundary
- **THEN** its original stable wire code and diagnostic message are retained; internal branches use typed identities

### Requirement: Automation preserves failure envelopes across execution paths
Shared failure conversion SHALL preserve domain codes for both immediate invocation and deferred polling, retain automation path/details, retain wire-validation paths, and preserve existing cancellation and generic exception classification. Operation IDs, schemas and provider lifetimes SHALL remain stable.

#### Scenario: Immediate and deferred failures
- **WHEN** the same coded domain failure occurs during invocation or a later Poll
- **THEN** both produce the same code/message classification without losing operation-owned cleanup or state guarantees

#### Scenario: Rich and generic errors
- **WHEN** an automation error has path/details, a wire error has a validation path, or an uncoded exception occurs
- **THEN** each retains its existing envelope and classification rather than being flattened into a domain error
