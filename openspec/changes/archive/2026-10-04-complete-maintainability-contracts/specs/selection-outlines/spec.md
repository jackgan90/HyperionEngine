## MODIFIED Requirements

### Requirement: Material coverage and display composition
Standard PBR masked geometry SHALL reuse its effective alpha texture, UV and cutoff inputs. Blended geometry SHALL use geometric coverage. Custom materials SHALL provide a compatible SilhouetteMask pass or report unsupported coverage. Fallback mask construction SHALL use the immutable material's resolved declared coverage policy rather than infer capability from shader paths or entry names. Legacy eligibility SHALL be resolved at material ingestion so existing assets retain coverage. Composition SHALL follow tonemapping, use correct color encoding, preserve output alpha and remain below GUI overlays.

#### Scenario: Masked surface
- **WHEN** a selected standard PBR surface has transparent cutouts and material overrides
- **THEN** the mask follows its effective coverage rather than the full underlying triangles

#### Scenario: Custom reflected inputs
- **WHEN** a custom SilhouetteMask pass uses reflected inputs with material or object overrides, including overrides used only by the ordinary color pass
- **THEN** the auxiliary material preserves the complete logical source interface, applies the effective clipping inputs, and produces binary coverage without rejecting inactive overrides or aborting the frame

#### Scenario: Exposure and resize
- **WHEN** exposure, viewport dimensions or depth convention changes
- **THEN** orange display color and pixel width remain stable, targets match the viewport, and no stale outline is retained

#### Scenario: Explicit fallback policy
- **WHEN** an otherwise identical pass enables or disables fallback, or supplies an explicit mask pass
- **THEN** coverage follows the declared policy or explicit mask and does not change solely because a source filename happens to match a built-in shader
