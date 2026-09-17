## Context

Editor already owns a value `FSceneCameraView`, but initializes it by copying `DefaultCamera`. SceneViewer still navigates the default camera object. Scene camera metadata and generation-checked references already reach Render, and render requests accept independent view values. Scene schema 6 has optional default-camera selection but no browsing preset. Existing component/editor work remains uncommitted and must be preserved.

## Goals / Non-Goals

**Goals:** Independent temporary browsing in both hosts; explicitly saved initial views; optional authored cameras with unambiguous preview; unchanged Main/Render ownership; compatible reads; guarded Sponza publication with unchanged model/material/texture dependencies.

**Non-Goals:** Pilot mode, picture-in-picture, camera gizmos, gameplay camera orchestration, automatic migration of scene topology, unrelated refactoring, and commits.

## Decisions

- Reuse the Scene-owned `FSceneCameraView` value for optional `InitialView` tool metadata in `FSceneSettings` and `FSceneManifest`. Reflect and validate its pose/lens. Increment the scene schema; old reads default to no preset without deleting or modifying authored cameras. Settings transactions and document history govern explicit preset changes. Ordinary save snapshots the stored preset and never samples navigation state.
- A shared Renderer helper initializes a browsing view from the preset or a deterministic frame of loaded model bounds, with a stable empty-scene fallback. Neither host copies the runtime default camera implicitly. SceneViewer uses the value overload of the existing navigation controller, so input no longer modifies authored objects or scene revisions.
- Editor explicitly tracks an optional preview-camera handle. The independent editor view remains untouched while previewing. Preview navigation and Frame Scene are disabled; property Apply uses the existing scene transaction. Exiting restores the preserved editor view. Disabled/missing/removed-component targets show an unavailable message and a cleared scene output, with a visible return action.
- Add a strict scene-camera request policy for preview; preserve existing default-camera fallback behavior for other callers. Render continues to resolve immutable, token-matched metadata. No UI reads Render-owned objects.
- Provide Set Initial View, preview/source selection, Return to Editor, Apply Editor View to Camera, and Create Camera from Editor View. Explicit authored writes use validation, dirty state and undo/redo. Applying a world pose to a parented camera derives a local matrix through the parent inverse and rejects singular parents before changing anything. Selecting a different preview target rejects uncommitted component drafts; returning to the editor view preserves them.
- Keep runtime default-camera references optional. Do not make rendering hosts add placeholder camera nodes merely to navigate. Existing compatibility helpers that explicitly request default authored content remain available; generic import/browser defaults use optional preset metadata or deterministic framing instead of synthesizing a camera object.

## Risks / Trade-offs

- Removing Sponza's camera could break SceneViewer or input replay -> switch its view source and assert camera-free navigation/save behavior before publication.
- Preview could show another camera after disabling its target -> opt out of renderer fallback and test disable/delete/remove/re-enable transitions.
- Viewport navigation could leak into saves -> compare snapshots and revisions before/after navigation, and reopen explicit preset saves.
- Camera creation and settings-only edits extend history -> cover undo/redo, subsequent edits, save points and asset reload with actual editor controls.
- Startup framing differs for older scenes without presets -> preserve their camera objects unchanged; use explicit presets for authored browsing positions and migrate shipped Sponza's previous view exactly.

## Migration Plan

Export and back up current Sponza and source/publication metadata. Verify the named default camera is enabled, has only Transform/Camera components, has no children or other references, and matches the source recipe. Move its world pose/lens into InitialView, remove only that object, clear DefaultCamera, and validate. Stage the scene and catalog with existing dependencies; compare scene content and rendered output, then publish scene/catalog/source metadata with old-value guards. Keep immutable dependencies byte-identical and retain rollback copies. Do not rewrite other scenes on load.

## Open Questions

None blocking the agreed first iteration. Advanced piloting and secondary preview rendering remain follow-up work.
