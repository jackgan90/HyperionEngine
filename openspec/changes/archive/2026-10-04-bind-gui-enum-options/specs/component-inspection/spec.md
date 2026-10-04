## ADDED Requirements

### Requirement: Explicit inspected choice values
Inspection choices SHALL associate each label with an explicit archive value. Display order and spelling SHALL NOT determine the edited value. Ordinary and mixed-selection inspectors SHALL use the same mapping; inspection validation SHALL reject values absent from the declared choices without changing the source object.

#### Scenario: Reordered sparse choices
- **WHEN** choices with non-contiguous values are reordered or relabeled
- **THEN** selecting a choice writes its declared value and existing values select the matching entry

#### Scenario: Mixed selection
- **WHEN** mixed selected objects receive a choice edit
- **THEN** the shared transaction receives the explicit selected value for every compatible target

#### Scenario: Unknown choice value
- **WHEN** an inspection draft contains a value absent from its choice declaration
- **THEN** applying the draft is rejected without modifying the source
