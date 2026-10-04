# scene-light-shadow-properties Specification

## Purpose
Define scene-owned directional and contact shadow authoring, persistence and immutable rendering precedence from the priority-resolved directional shadow source.
## Requirements
### Requirement: Authored directional light shadows
DirectionalLight SHALL support optional CPU-owned directional-map and contact-shadow settings with shared validation, reflection, native persistence and component history. Old scene records without these fields SHALL load without modification and retain session defaults. Point/spot shadow authoring SHALL remain unavailable; value structures SHALL keep shadow methods distinct for future extension.

#### Scenario: Edit and reopen shadows
- **WHEN** light shadow settings are edited through Details or Automation, undone/redone, saved and reopened
- **THEN** the same validated component values and scene dirty/history semantics apply

#### Scenario: Invalid or unsupported authoring
- **WHEN** invalid shadow ranges or point/spot shadow fields are submitted
- **THEN** the request fails without partial mutation

### Requirement: Published light shadow precedence
Renderer SHALL consume authored shadow values from the priority-resolved directional shadow source in the immutable scene publication. Authored values SHALL take precedence over session defaults; a source without overrides SHALL restore defaults. Contact shadows and CSM SHALL use the same source with their independent method switches. Host-only preview placement SHALL not be serialized into light data.

#### Scenario: Switch priority winner
- **WHEN** priorities select a different eligible light with or without authored shadow settings
- **THEN** both shadow consumers use its direction and settings without leaking previous values

### Requirement: Stable typed shadow preview identities
Directional and contact preview choices SHALL have explicit typed identities, stable numeric encodings and display labels in a shared CPU-only contract. Existing field names, record versions and numeric schema shapes SHALL remain unchanged. Unknown numeric values SHALL be rejected before mutation.

#### Scenario: Existing authored modes
- **WHEN** any existing directional value 0 through 5 or contact value 0 through 2 is saved, loaded or invoked through automation
- **THEN** its previous meaning and numeric representation are preserved

#### Scenario: Presentation reordering
- **WHEN** preview labels are reordered or relabeled
- **THEN** the choice retains its original numeric value and rendering meaning
