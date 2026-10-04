# typed-runtime-option-state Specification

## Purpose
Define typed runtime rendering options and profiling HUD selections while preserving external contracts and frame and preview behavior.
## Requirements
### Requirement: Runtime option state uses owning identities
Configuration-derived pipeline, GBuffer and debug choices and runtime shadow/visualizer state SHALL use existing domain enums. Viewport profiling HUD selections SHALL use named typed flags with fixed protocol bits independent of presentation order. External numeric/string DTOs SHALL be converted at their owning boundaries without duplicating option mappings.

#### Scenario: Configuration and reflection compatibility
- **WHEN** legacy settings are loaded, described, changed through GUI/automation, saved and reopened
- **THEN** existing scalar kinds, field names/order, defaults, ranges and revision semantics are preserved while consumers receive typed values

#### Scenario: Invalid and unsupported options
- **WHEN** an unmapped option, unknown HUD bit, invalid typed identity or unsupported viewport field is supplied
- **THEN** existing provider availability and viewport unsupported-before-value-validation precedence rejects it without partial state changes, truncated values or a substituted default

#### Scenario: Reordered HUD presentation
- **WHEN** category labels are reordered or relabeled
- **THEN** the same selected category retains its fixed wire bit and displayed diagnostic content

#### Scenario: Multiple invalid semantic fields
- **WHEN** a render-settings request contains both an invalid top-level option and an invalid nested contact setting
- **THEN** it is rejected before provider invocation with the existing error code, path and details; typed field conversion MAY select a different first diagnostic message among the invalid semantic fields

### Requirement: Frame and preview behavior remains stable
Typed option propagation SHALL preserve existing main viewport and asset-preview setting boundaries, owned frame snapshots, shader codes and resource retirement. Configuration/provider selection SHALL remain independent of optional graphics consumers.

#### Scenario: Main and asset previews
- **WHEN** live rendering options change with existing, later-opened or resumed previews
- **THEN** main settings and preview-specific defaults/exposure behave as before and shared depth-convention changes apply without changing document history
