## Context

Editor owns one shared FSceneEditDocument and exposes ISceneDocumentHost. Empty scene instances already support authoring and native persistence. FEditorDocumentTransition currently uses an empty PendingOpen string to mean no open intent; save continuations only cover root switching and application exit. Closed scene instances reject render publication and asset refresh access.

## Goals / Non-Goals

**Goals:** Explicit new and close actions, scene-scoped save/discard/cancel protection, usable closed-document GUI, reflected automation parity, safe task/resource retirement and compatible existing operations.

**Non-Goals:** Default scene objects, multiple scene tabs, content-root switching, new transports or plugin lifecycle, asset format changes, archiving or committing this change.

## Decisions

1. Add typed scene lifecycle actions and reflected dirty-decision requests to SceneEditing's host contract. GUI requests and automation bindings call the same host validation and transition execution. The host retains a Main-owned completion record until the scene is ready, closed or failed; consumers poll the same record. This avoids implementing orchestration in transports or exposing GUI state.
2. Extend FEditorDocumentTransition with an optional scene intent and explicit phases. New and Close are not represented by an empty path. Save captures the original scene, waits for completion and rechecks dirty/document identity before continuation. Cancel withdraws the intent but never cancels admitted IO. Existing Open path remains compatible, including empty-path automation recovery.
3. A fresh New scene is loaded, zero-node, untitled and initially clean. Browsing uses an editor camera override without authoring a camera/light. Close leaves the editing target attached to the closed instance for status queries, invalidates the document epoch and clears transient scene state. New/Open subsequently restores an editable target. Closed is distinct from empty loaded, loading and failed states through the owning instance status.
4. Keep asset tabs and the selected content root independent. Scene close joins scene work and retains the existing GPU fence retirement. Render publication, placement, scene panels, view actions and asset-to-scene refresh check scene availability; a closed viewport offers New/Open and cannot sample an old image.
   Place Object treats the closed scene as a normal inactive state: entries remain disabled, while preparation prompts and old placement feedback are hidden. Shared placement availability and automation admission continue to reject unavailable scenes.
5. New/Close dirty dialogs reuse Unsaved changes and FWindowGroup. Their Save/Discard applies only to the scene. File Save and Ctrl+S share untitled Save As routing. Automation defaults to reject-dirty, allows explicit save (with scenePath for untitled) or discard, and declares actual completion. Existing scene.open IDs/fields and application.close state remain unchanged.

## Risks / Trade-offs

- Save completes after cancellation or replacement -> completion records and document epochs prevent revived intents and incorrect saved state.
- GUI changes an interaction while a request is queued -> finish GUI edits before admission, block scene mutation during continuation and recheck identity/revision/dirty.
- Asset saves while no scene exists -> continue workspace polling and browser refresh while skipping closed-scene refresh.
- Modal overlaps root or exit -> retain existing exit/root priority and explicitly cancel superseded scene intents; regress existing tests.
- Close returns loaded=false/ready=false -> its automation completion uses the lifecycle record, not the existing open adapter's ready predicate.

## Migration Plan

Add contracts and transitions, integrate Editor and adapters, then verify unit/real GUI/attached automation and Debug/Release builds. No serialized migration is required. Leave all artifacts in the active change directory and do not archive or commit.

## Open Questions

None; startup behavior remains the existing empty document, and fresh unedited New scenes remain clean.
