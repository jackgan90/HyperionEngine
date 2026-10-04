## ADDED Requirements

### Requirement: Stable typed shadow preview identities
Directional and contact preview choices SHALL have explicit typed identities, stable numeric encodings and display labels in a shared CPU-only contract. Existing field names, record versions and numeric schema shapes SHALL remain unchanged. Unknown numeric values SHALL be rejected before mutation.

#### Scenario: Existing authored modes
- **WHEN** any existing directional value 0 through 5 or contact value 0 through 2 is saved, loaded or invoked through automation
- **THEN** its previous meaning and numeric representation are preserved

#### Scenario: Presentation reordering
- **WHEN** preview labels are reordered or relabeled
- **THEN** the choice retains its original numeric value and rendering meaning
