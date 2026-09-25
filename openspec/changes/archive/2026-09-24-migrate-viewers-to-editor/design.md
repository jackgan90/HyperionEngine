## Context

The maintained Editor already shares native loading, asset editing, scene transactions, navigation, capture and automation. Missing controls live in the early inspection plugins or their shared application. Many GPU acceptance scripts launch that application; simply deleting targets would silently reduce coverage. The working tree was clean before this change. The separate simplify-reflection-declarations proposal is outside this scope.

## Goals / Non-Goals

Goals: a single maintained graphics application; reusable engine-owned controls; complete GUI/agent editing parity; preserve rendering/resource/lifecycle coverage; remove obsolete source, composition and user documentation.

Non-goals: new import formats, runtime plugin loading, rewriting renderer algorithms, a second production inspection host, changing default Editor scheduling or selection semantics, Git commits.

## Decisions

1. Extend existing SceneEditing history for primary-node duplication and keep-children deletion. Capture affected nodes/settings/selection before mutation; restore resource references and remap recreated handles through all history. Reparent and scene settings consume existing document methods. All GUI and agent changes use this authority.
2. Runtime/Renderer owns render control values, validation and debug geometry. Editor owns service instances and UI, publishing immutable settings into rendering. Scene/Environment remain CPU-only. Temporary debug options do not dirty scene documents. Authoring lights and skies uses document history.
3. Keep native asset documents, navigation, capture and output APIs. Add missing controls to the existing viewport and diagnostic interfaces. Retain generic operation IDs and wire semantics. Retired experimental startup fields are not silently accepted as new behavior.
4. Preserve runtime frame-pipeline implementations and focused GPU tests. Move useful integration scenarios to Editor or a test-only harness exercising public runtime APIs; do not relocate an unchanged legacy application under a new name. Migrate benchmark consumers and compare viewport images at matched dimensions and settings rather than application chrome.
5. Remove old targets and build switches only after replacements exist. CTest registration is capability-specific instead of an early return to a reduced suite. Editor becomes VS startup target. Optional capture/profiling/feature absence remains controlled and testable.
6. Delete obsolete documentation; rewrite active mixed documentation around current capabilities. Historical measurements must never be relabelled as new Editor results. Existing archive evidence is historical, not a build dependency; inventory it separately during cleanup, preserving unrelated history and avoiding fabricated evidence.

## Risks / Trade-offs

- Structural restoration can invalidate handles -> remap selection, settings, prior and future history; test repeated undo/redo and shared resource identity.
- Debug camera and culling camera can diverge -> freeze only spatial rejection, retain current display/shadow camera, and clear frozen state on document replacement.
- GUI coordinates differ from full-window rendering -> use explicit viewport clipping and pixel dimensions for debug overlays and acceptance.
- Removing launchers can hide regressions -> record old test inventory and maintain a coverage mapping; run complete remaining suites and new parity cases.
- Startup depth and capture choices cannot hot reload -> expose requested versus active state and require restart where appropriate.

## Migration Plan

Record baseline, implement shared editing and rendering services, integrate Editor/Automation, port tools and tests, remove production legacy composition and docs, validate default and reduced builds, synchronize specifications and archive. Changes remain uncommitted and reviewable.
