## 1. Implementation

- [x] 1.1 Add shared capture overlay query/set and optional startup setting.
- [x] 1.2 Add backward-compatible persisted Editor HUD preference and live GUI control.
- [x] 1.3 Register reflected automation HUD get/set through the shared domain interface.

## 2. Validation and documentation

- [x] 2.1 Cover preference compatibility, API mask behavior, GUI persistence and capture, automation discovery/invocation and failure paths.
- [x] 2.2 Update user and automation documentation.
- [x] 2.3 Run focused builds/tests, style/boundary checks and OpenSpec validation; record evidence.

## Validation evidence

- Debug build with HYP_ENABLE_RENDERDOC=ON passed; Release Editor, automation CLI and preference tests with HYP_ENABLE_RENDERDOC=OFF built and linked successfully.
- Debug: editor_preferences, editor_capture_ui (including hidden-HUD RDC replay/XML), automation_renderdoc_hud, automation_scene, automation_contracts, plugin_runtime, capture_controls, capture_unavailable, capture_incompatible, capture_runtime and capture_failure_recovery passed. No RenderDoc cases were skipped on this machine.
- Release: editor_preferences, editor_capture_ui and automation_renderdoc_hud passed with capture support uncompiled.
- The new automation fixture initially conflicted with the Engine physical mount and queried Editor operations on a standalone asset catalog. Corrected it to use an isolated Game directory and attached Editor catalog; provider absence is covered in automation_scene. Final reruns passed.
- Full CheckStyle formatting/path checks and CheckBoundaries passed; semantic naming passed for all 14 modified C++ translation units. git diff --check and openspec validate editor-renderdoc-hud --strict passed.
- Implementation validation completed before the separately authorized archive and local commit.
