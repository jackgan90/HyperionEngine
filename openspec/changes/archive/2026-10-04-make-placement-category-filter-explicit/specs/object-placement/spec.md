## ADDED Requirements

### Requirement: Explicit placement category filtering
Placement queries SHALL represent unrestricted filtering explicitly and SHALL treat every supplied category ID as an exact extensible string identity. Display text SHALL NOT determine unrestricted behavior. Registry ordering, keyword search and unique object results SHALL remain unchanged.

#### Scenario: Unrestricted and category-specific queries
- **WHEN** a caller selects unrestricted filtering or a registered category
- **THEN** the former returns all keyword-matching objects once and the latter returns only matching category members

#### Scenario: Category named All
- **WHEN** a category with ID All is registered and selected
- **THEN** it behaves as an ordinary category independently of the palette's unrestricted All button and their controls have distinct identities

#### Scenario: Unknown and empty supplied category
- **WHEN** a supplied category has no registered members, including an empty supplied string
- **THEN** the query returns no objects instead of silently becoming unrestricted
