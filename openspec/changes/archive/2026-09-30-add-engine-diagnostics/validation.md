# Validation

Validated on 2026-09-29 on Windows with the Debug and Release build configurations.

## Build and regression results

- Both configurations built the Editor, automation CLI and affected plugin, import, scene, shader, render-resource and D3D12 regression targets successfully.
- Both configurations passed all 10 selected CTest suites: `automation_transport`, `graphics_shutdown_validation`, `editor_log`, `scene_runtime_instance`, `asset_import_workspace`, `plugin_runtime`, `plugin_applications`, `shaders`, `render_resources`, and `d3d12_frame_failure_recovery`.
- Source formatting and filename/include checks passed for 939 owned source files.
- Dependency-boundary checks passed for 902 source files across 38 modules.
- Semantic naming and local-declaration checks passed for all 26 changed translation units.
- `git diff --check` passed.
- `openspec validate --all --strict` passed all 101 items.

## Diagnostic behavior covered

- Plugin startup failures retain their existing identifying prefix and report an Error; cleanup failures report the plugin and original cause. A deliberately throwing log output callback does not stop remaining cleanup or replace the original cleanup exception.
- Content-root retirement failure reports previous and target directories, the failed stage, original cause and required host restart.
- Import terminal records contain task, source and output context. Successful and stale-revision asset saves report the asset/path and outcome. Repeated updates and save polling add no records.
- Scene load failures identify the scene and epoch once; successful and failed scene saves report the path and outcome. Repeated ticks and polling remain quiet.
- Render-resource preparation failure reports the resource identity and cause once; status queries do not repeat it.
- A real DXC truncation warning includes the source, entry point, profile and target. Loading the resulting shader from cache does not replay it.
- Injected D3D12 Warning, Error and Corruption messages are visible with their native identifiers. Identical messages log once, distinct text remains visible, and validation error counts still include every native Error/Corruption occurrence across repeated statistics queries.
- Existing Editor Log and automation transport acceptance suites remain green, exercising the shared log channel and command-line transport behavior.

## Scope and delivery state

Diagnostics are added at selected shared lifecycle, terminal-operation and failure-publication boundaries. This change does not claim exhaustive instrumentation of every exception or validation branch, and adds no per-frame tracing or new automation operations.

Implementation was initially delivered unarchived and uncommitted. Archive, main-spec synchronization and a local Git commit were subsequently authorized on 2026-09-30.
