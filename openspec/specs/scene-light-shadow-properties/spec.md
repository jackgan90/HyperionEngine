# scene-light-shadow-properties Specification

## Purpose
Define scene-owned directional and contact shadow authoring, compatible persistence and immutable main-light rendering precedence.
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
Renderer SHALL consume authored shadow values from the current main directional light's immutable scene publication. Authored values SHALL take precedence over compatible legacy session defaults; switching to a light without overrides SHALL restore those defaults. Host-only preview placement SHALL not be serialized into light data.

#### Scenario: Switch main directional light
- **WHEN** main-light selection changes between authored and legacy lights
- **THEN** rendering uses the selected light's shadow settings or the original session defaults without leaking previous values
