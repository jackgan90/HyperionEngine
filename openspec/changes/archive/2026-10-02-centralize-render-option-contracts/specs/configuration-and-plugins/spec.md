## MODIFIED Requirements

### Requirement: Validated configuration loading
Loading SHALL reject incompatible schema versions, invalid types and out-of-range fields before replacing application settings. Existing raster pipeline, GBuffer preset and visualizer fields SHALL use the shared CPU raster option contract in both legacy reflected configuration and record-based configuration paths. Unmapped option values SHALL fail before replacement or persistence rather than falling back to another supported choice; existing field names, wire kinds, versions, defaults and valid ranges SHALL remain compatible. The generic reflected read/write path SHALL support optional pure-value property validation supplied by Config without a Reflection dependency on the raster option domain. DecodeReflected and LoadReflected SHALL complete this validation for all pending properties before invoking any setter; EncodeReflected SHALL perform the same validation before SaveReflected writes or replaces files. Config's direct option setters SHALL validate before assignment. These preflight guarantees cover parsing and pure-value validation rejection and do not add rollback for arbitrary setter exceptions.

#### Scenario: Invalid input
- **WHEN** a configuration contains an invalid window width or a future version
- **THEN** loading fails with an actionable error and the settings remain unchanged

#### Scenario: Unmapped raster option
- **WHEN** either supported configuration path loads or attempts to save an unknown pipeline/GBuffer token or invalid visualizer value
- **THEN** shared option validation reports the invalid value without replacing valid settings or writing an invalid settings file

#### Scenario: Direct reflected loading preserves all existing fields
- **WHEN** DecodeReflected or LoadReflected targets an existing valid settings object with an earlier valid title change and a later unknown pipeline or GBuffer token
- **THEN** validation fails before any setter runs and every field of that existing object remains unchanged
- **AND** direct option property assignment and record-based loading also reject unmapped values before changing the corresponding settings

#### Scenario: Direct reflected saving preserves the existing file
- **WHEN** EncodeReflected, SaveReflected or SaveSettings receives settings containing an unmapped raster option
- **THEN** encoding rejects the invalid value through the shared pure-value validation
- **AND** either file-saving entry leaves an existing target file byte-for-byte unchanged

#### Scenario: Properties without custom validation
- **WHEN** an existing reflected property has no optional pure-value validation callback
- **THEN** its existing type/range validation, assignment and encoding behavior remains unchanged

#### Scenario: Configuration without optional rendering plugins
- **WHEN** the application is built without optional rendering UI plugins or starts with graphics/editor plugins disabled
- **THEN** loading valid raster option values requires no Renderer/RHI dependency from Config and does not activate a disabled plugin
