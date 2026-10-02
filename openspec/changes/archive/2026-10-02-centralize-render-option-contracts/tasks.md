## 1. Freeze compatibility evidence and introduce the CPU contract

- [x] 1.1 Recheck the current source consumers and capture existing config/wire examples plus official render.settings/view operation and type schemas, versions, defaults and failure semantics as fixed compatibility expectations.
- [x] 1.2 Add Runtime/RasterOptions and hyperion_raster_options with stable typed pipeline/preset/visualizer identities, immutable descriptors, strict parse/serialize/encoding helpers, defaults and order-independent selection helpers; preserve ESceneRenderPipeline source identity and numeric values.
- [x] 1.3 Add focused CPU tests for every fixed token/wire mapping, all visualizer meanings, descriptor uniqueness/completeness, reordered descriptions, unmapped enum/token/value rejection and signed negative debug rejection.
- [x] 1.4 Register the module before Config, declare the required direct CMake dependencies and extend CheckBoundaries to enforce RasterOptions' zero engine dependencies without loosening other module rules.

## 2. Migrate configuration and renderer boundaries

- [x] 2.1 Add an optional pure-value Validate callback at the end of FProperty; invoke it from generic ReadValue after primitive conversion/range checks so all DecodeReflected pending values are validated before any setter and EncodeReflected validates before SaveReflected touches files. Preserve properties without callbacks and add no Reflection dependency on RasterOptions or domain-specific branch; do not introduce arbitrary setter-exception rollback.
- [x] 2.2 Migrate Config's SettingsType callbacks and direct field setters, LoadSettings/SaveSettings and RecordType validation to the shared contract, reusing Config-owned pure validators before assignment and preserving both serialization paths, property keys, kinds, defaults and unrelated ranges.
- [x] 2.3 Migrate ValidateRenderSettings, direct MakePipelineSettings, SceneRenderPipeline::Configure and ValidateViewportOptions to strict shared mapping; retain the complete FGBufferLayout/device validation boundary and existing string/uint DTOs.
- [x] 2.4 Extend configuration_plugins with generic reflection validation cases and extend render_controls: prove all pure validation precedes any setter and absent callbacks preserve behavior; exercise direct DecodeReflected and LoadReflected with an earlier valid title edit plus invalid pipeline and GBuffer tokens, asserting every existing field remains unchanged. Cover direct EncodeReflected rejection, unchanged existing file bytes after direct SaveReflected and SaveSettings rejection, direct setter/record rejection, both formats and full round trips, direct conversion/configuration rejection, and VSync-only edits for every supported pipeline/preset combination.

## 3. Connect stable visualizer IDs to the production shader

- [x] 3.1 Implement MakeGBufferVisualizerShaderDefines in Renderer with its declaration beside DeferredLighting shader parameters; derive canonical name/value pairs from RasterOptions, excluding labels and presentation order.
- [x] 3.2 Build the GBuffer debug material's Pixel.Defines through that adapter, encode DebugMode through the named conversion boundary, and replace Debug.hlsl numeric mode comparisons with required named defines and explicit missing-definition compilation errors.
- [x] 3.3 Make DeferredShaderTests consume the same production define adapter and verify DXIL/SPIR-V/MSL output/reflection, missing-definition rejection, fixed expected define values, and cache miss/hit behavior for changed/restored semantic inputs; verify reordered/relabeled presentation leaves canonical defines unchanged.
- [x] 3.4 Extend deferred_rendering with fixed raw modes 0–6 and independent pixel expectations for base color, distinct normals, metallic/roughness/AO, emissive and depth; cover compact/high, Standard/Reversed Z, display-order equivalence and documented sRGB/quantization tolerances.

## 4. Migrate GUI consumers without changing shared operation behavior

- [x] 4.1 Migrate Editor render-setting combos and visualizer selector/HUD labels to stable-ID descriptor lookup, writing only the specifically changed setting and preserving the existing visible choices.
- [x] 4.2 Migrate Editor pipeline availability checks and optional DebugUI pipeline/GBuffer/visualizer controls to shared typed mappings; preserve Forward latent visualizer behavior and existing settings/viewport service ownership.
- [x] 4.3 Extend semantic GUI acceptance to select supported pipeline/preset/visualizer choices and toggle VSync independently; assert actual committed values, saved/reopened values, no scene history changes and valid HUD labels.
- [x] 4.4 Verify the main viewport consumes the complete committed render settings; existing/new/resumed 3D asset previews share only the committed depth convention. Assert that main pipeline/preset/visualizer/exposure changes preserve preview Deferred/Compact/Lit defaults and each Entry's own exposure, and main VSync edits preserve the asset-window throttling policy derived from main-window drawability and exercise/benchmark mode. Retain the respective owned frame paths, resource reuse and old-frame/fence lifetime regressions.

## 5. Verify official automation and optional capability boundaries

- [x] 5.1 Extend official api.search/api.describe/types.describe coverage for render.settings.get/set/save and view.get/set; compare operation versions and record property shapes/constraints against fixed pre-change expectations.
- [x] 5.2 Exercise real get/set/save and view invocation for all stable options, VSync-only edits, invalid tokens/values, stale revision, Forward latent selection, save/reopen and missing-provider unavailability; assert rejected calls do not change state or revision.
- [x] 5.3 Build an isolated configuration with BUILD_TESTING=OFF and HYP_ENABLE_DEBUG_UI/HYP_ENABLE_TRIANGLE/HYP_ENABLE_RENDERDOC=OFF, compiling/linking Editor, automation CLI and asset tool; separately verify RasterOptions/Config target dependencies remain CPU-only.
- [x] 5.4 Run plugin_runtime/plugin_applications and affected automation provider-absence cases, including kernel-only and explicitly disabled graphics/gui/editor/contact-shadows, and verify no stored option implicitly activates an unavailable plugin.

## 6. Final validation and independent review

- [x] 6.1 Complete Debug and Release builds and the focused CPU/shader/automation tests: new raster-option tests, configuration_plugins, render_controls, shaders and automation_scene; record actual outcomes and any environment limits.
- [x] 6.2 Run GPU/desktop tests serially: deferred_rendering, editor_render_controls, editor_render_acceptance and affected asset-preview/lifetime regressions; verify fixed pixel expectations, zero validation errors and unchanged stable-frame resource/material reuse.
- [x] 6.3 Run applicable CheckStyle format/path/naming checks, CheckBoundaries and strict OpenSpec validation on the final change; update SourceLayout/Materials/Editor documentation and ensure the existing automation coverage inventory remains accurate.
- [x] 6.4 Obtain an independent review covering mapping authority, generic reflected read/write preflight and direct setters, schema/wire compatibility, GUI field preservation, main-versus-preview settings ownership, shader input generation/cache identity, optional build paths and owned-resource lifetimes; verify findings directly and resolve confirmed issues before marking implementation complete.
