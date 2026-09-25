# Capability and retirement evidence

The approved boundary is the independent scene/model applications and their host composition. Shared Runtime algorithms remain supported. Each replacement must pass before its old consumer is removed.

| Retired consumer | Supported implementation and regression coverage |
| --- | --- |
| SceneViewer scene loading, navigation and save | SceneInstance, SceneEditing and Editor; scene_navigation, scene_runtime_instance, automation_scene, editor_acceptance |
| ModelViewer asynchronous native loading, camera and material pixels | Native assets and Editor AssetWorkspace/AssetPreview; model_rendering preserves gated IO, readiness, resize and pixel checks; automation_parity_regressions covers preview failure and camera |
| Scene structural controls | SceneEditing transactions shared by Editor and Automation; duplicate and keep-children undo/redo, remapped handles, settings and resource identity covered by automation_scene and scene_navigation |
| Viewer render/shadow configuration and diagnostics | Renderer RenderSettings, ShadowControls and DebugGeometry; Editor diagnostics and typed Automation operations; render_controls and editor_render_acceptance |
| Viewer benchmark CSV and camera/light workload | Renderer RenderBenchmark and Editor benchmark host; editor_render_acceptance, MeasureDeferred.py, MeasureShadows.py |
| Viewer profiling setup and controls | Core ProfilingSession and IProfilingControl; Editor startup/Automation; profiling_contracts and optional profiling_trace_acceptance |
| Viewer RenderDoc orchestration | Existing ApplicationServices capture service and Editor capture UI; editor_capture_ui, capture_controls, capture runtime/failure tests |
| Viewer CPU frame queue CLI | Public FramePipeline remains; cpu_frame_pipeline and cpu_frame_ownership cover queue limits, skipped ticks, pixels, result ownership and failures |
| Viewer host plugin startup | Editor plugin_applications tests kernel, explicit disablement and requested unavailable output; plugin_runtime tests unknown plugins, dependency selection and lifecycle |
| Viewer triangle/clear/GUI/window smoke tests | Independent Runtime/RHI rendering, gui_texture_rendering, window ownership/input, lifecycle_recovery and Editor acceptance; Triangle remains an optional independent sample plugin |
| SceneViewer hardcoded sky folder controls | Retired host adapter; native sky assets, Editor property editing and environment_preprocessing/sky_rendering remain |
| Viewer-only application.settings operation and IApplicationSettings | Sole concrete provider is FViewerPlugin (Source/Plugins/Viewer/Private/ViewerApplication.h); references outside that plugin are only schema/Automation registration and CMake. Replaced with typed render.settings operations, Editor preferences and content-root operations. No other implementation consumes the interface. FAppSettings and Config remain for independent sample plugin tests. |

The tracked directories Source/Applications/Viewer and Source/Plugins/Viewer, SceneViewer, ModelViewer contain only the retiring host/plugins. No Runtime directory is removed. ApplicationSettings retirement is limited to its interface, record and adapter plus registrations; shared Config and test infrastructure are preserved. PluginProfiles.cmake's early-return reduced suite is replaced by per-capability test registration in the main test list, preserving unrelated tests in optional builds.

Host-specific fixed animation/move keys, first-loaded Add, replacement selection, edited-file suffixes, startup model/scene configuration and preset JSON files are intentionally retired. Historical benchmark reports tied to that host are removed rather than relabeled as Editor measurements. Shared renderer contracts in active documentation/specifications are updated to refer to their current owners.
