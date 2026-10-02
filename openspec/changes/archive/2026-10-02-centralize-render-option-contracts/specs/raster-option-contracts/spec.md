## ADDED Requirements

### Requirement: CPU-owned raster option identities
The engine SHALL provide one CPU-only raster option contract for supported scene pipelines, GBuffer presets and GBuffer visualizers. It SHALL associate typed identities with stable configuration tokens or wire values and display labels without deriving identity from presentation order. Config, Renderer and affected GUI consumers SHALL use this contract. The owner SHALL NOT depend on Renderer, RHI, native backends, concrete plugins or active services.

#### Scenario: Reordered presentation
- **WHEN** supported option descriptions are presented in a different order
- **THEN** lookup, selection, serialization and rendering preserve the same stable identity for every option
- **AND** descriptor validation rejects duplicate or missing identities and ambiguous tokens

#### Scenario: Optional graphics consumers absent
- **WHEN** optional DebugUI, Triangle and RenderDoc plugins are omitted from the build or graphics/editor services are unavailable at runtime
- **THEN** CPU option/configuration validation remains usable without activating those plugins
- **AND** unsupported rendering operations retain controlled unavailability

### Requirement: Strict option boundary conversion
Configuration loading, reflected option assignment, live render settings, viewport option validation and direct pipeline configuration/conversion SHALL reject unmapped tokens, numeric values and typed identities. They SHALL preserve the existing default choices and the complete GBuffer layout API. Invalid input SHALL NOT silently select a default or partially replace committed settings. Generic reflected configuration decoding SHALL validate all pending values before any assignment, and encoding SHALL validate before a file-saving entry writes or replaces a target. These guarantees concern invalid input rejected during preflight and do not require rollback for arbitrary setter exceptions.

#### Scenario: Direct conversion rejects an unknown token
- **WHEN** MakePipelineSettings is called directly with an unknown pipeline or GBuffer token
- **THEN** it reports an invalid argument instead of returning Deferred or Compact as a fallback

#### Scenario: Invalid numeric visualizer
- **WHEN** a caller supplies a negative signed application debug value, an unmapped unsigned visualizer, or an invalid typed option
- **THEN** validation rejects the input before conversion, settings publication or shader parameter encoding

#### Scenario: Generic reflected entries reject before mutation
- **WHEN** a caller uses direct reflected decoding/loading with earlier valid field changes followed by an unmapped option, or direct reflected encoding/saving with an invalid option
- **THEN** pure-value validation rejects the operation before any target field is assigned or an existing settings file is written or replaced

#### Scenario: Existing choices remain valid
- **WHEN** existing deferred/forward, compact/high and visualizer values 0 through 6 are loaded, converted and saved
- **THEN** their values, defaults and existing device-capability checks remain unchanged
- **AND** custom complete GBuffer layouts retain their existing validation path

### Requirement: Stable visualizer CPU and shader protocol
The visualizer contract SHALL retain 0 Lit, 1 Base color, 2 Shading normal, 3 Metallic/Roughness/AO, 4 Emissive, 5 Scene depth and 6 Geometry normal. Shader codes SHALL be obtained through an explicit encoding boundary, independent of display position. The production GBuffer debug material SHALL obtain named shader defines from the same contract through the existing material shader define mechanism. HLSL SHALL use those names and fail compilation when required definitions are absent.

#### Scenario: Production and direct shader compilation
- **WHEN** the production material or a direct shader compiler test compiles Deferred/Debug.hlsl
- **THEN** both consume the same production define adapter
- **AND** DXIL, SPIR-V and MSL compilation retain the GBufferDebugV1 reflection contract

#### Scenario: Missing visualizer definitions
- **WHEN** a required visualizer define is missing from a Debug.hlsl compilation
- **THEN** compilation fails explicitly without substituting a hard-coded number

#### Scenario: Shader cache follows semantic inputs
- **WHEN** a visualizer shader define value changes in an isolated compile input
- **THEN** the shader cache identity changes
- **AND** reusing identical definitions can hit the cache, while only changing display labels or order does not change the canonical production define inputs

### Requirement: Independent visualizer meaning validation
Delivery SHALL validate visualizer meanings with fixed raw wire values and independent expected pixel results, not solely producer/consumer round trips or image differences between modes. Existing debug mathematics, GBuffer validity handling, output color encoding and Standard/Reversed Z behavior SHALL remain unchanged. GPU execution claims SHALL be limited to implemented native backends.

#### Scenario: Known GBuffer values
- **WHEN** an asymmetric fixture with known base color, material channels, emissive, distinct shading/geometry normals and depth is rendered for raw visualizer values 1 through 6
- **THEN** each mode matches its independently calculated expected pixel values within documented storage and color-encoding tolerances for compact and high precision layouts
- **AND** mode 0 follows the existing lit path

#### Scenario: Presentation order does not alter GPU meaning
- **WHEN** the same stable mode is selected from normal and reordered presentation descriptions
- **THEN** its uploaded shader code and rendered image are equivalent

### Requirement: Stable external shapes and lifecycle
Existing configuration property keys, record and operation IDs, schema versions, wire scalar kinds, field order, required/optional shapes, numeric constraints and live revision behavior SHALL remain compatible. Option changes SHALL continue through existing UI-independent services and owned frame snapshots. The main viewport SHALL consume complete committed render settings; 3D asset previews SHALL retain their independent Deferred pipeline, Compact GBuffer, Lit visualizer and per-entry exposure, while continuing the existing shared depth-convention and asset-window VSync behavior. Shader constant layout, material/resource reuse and GPU fence retirement SHALL remain intact.

#### Scenario: Official discovery and invocation
- **WHEN** clients use api.search, api.describe and types.describe followed by render.settings.get/set/save and view.get/set
- **THEN** they observe the existing operation versions and string/numeric wire contracts
- **AND** valid calls, invalid-argument rejection, stale revisions, save/reopen and missing-provider failures preserve the existing completion and state semantics

#### Scenario: Main viewport receives committed settings
- **WHEN** complete live render settings are committed while main viewport frames are queued
- **THEN** subsequent main viewport frames consume the committed settings through the existing owned snapshot path
- **AND** already owned frames remain valid until their normal completion and resource retirement

#### Scenario: Preview preserves its existing settings boundary
- **WHEN** main pipeline, GBuffer preset, visualizer or exposure changes while a 3D asset preview exists, is later opened or resumes
- **THEN** the preview retains Deferred, Compact, Lit and its own per-entry exposure independently of those main viewport values
- **AND** current, newly opened and resumed previews continue using the committed depth convention
- **AND** main VSync edits do not replace the existing asset-window VSync throttling policy based on main-window drawability and exercise/benchmark mode
- **AND** already owned preview frames and resources retain their normal completion, reuse and fence retirement behavior
