## Context

Editor draws a temporary root drop zone before the Outliner object table. Its reparent controller already delegates both root and node delivery to SceneEditing's atomic KeepWorld operation. Logical scenes support multiple parentless nodes and all object operations use real scene handles.

## Goals / Non-Goals

**Goals:** stable Outliner layout when a drag starts/ends, a discoverable permanent root destination, meaningful scene labeling, and preserved selection/history/persistence and GUI/automation equivalence.

**Non-Goals:** introducing a persisted root entity, migrating scene files, scene transforms/components, scene selection/settings UI, new automation operations, OpenSpec archival, or Git commits.

## Decisions

1. Represent the Scene row with an Editor-private scene-item type derived from document identity and display path. Its GUI identity is distinct from node identity and stable across labels. It has no scene handle, never enters displayed object rows or selection, and cannot originate reparent gestures.
2. Draw the scene item inside the object table, before roots or filtered objects, with Type `Scene`. Its tree is initially open, including empty scenes. Existing roots retain their real hierarchy. Entering/changing a nonempty search expands the container once; users can subsequently collapse it. Drag state does not control visibility, height, or expansion. Existing intentional hover expansion remains supported.
3. Separate root target routing from row rendering. The Outliner submits its scene item first, then the reparent controller routes that existing item to a null-parent target. The controller retains gesture, validation, feedback, delivery and cancellation ownership. Feedback uses the existing overlay/tooltip rather than extra layout rows.
4. Preserve `scene.nodes.reparent` and its optional-parent request unchanged. Root delivery, no-op handling, selected-ancestor normalization, KeepWorld, one history transaction and save/reopen are owned by the existing SceneEditing implementation. Presentation collapse/search has no domain mutation or new adapter; document this mapping in the automation coverage inventory.
5. Reuse the existing headless GUI/controller tests for stable bounds, scene-row exclusion, collapse/search/document identity and empty scenes. Extend actual Editor reparent acceptance for permanent-root/layout assertions; retain the existing selection, cancellation, persistence and automation checks.

Alternatives considered: a real root would introduce transform/enable inheritance and root protection/migration rules unrelated to this interaction. Keeping a permanently visible action label outside the table solves the jump but does not express the requested scene hierarchy.

## Risks / Trade-offs

- [Container accidentally selected as an object] -> dedicated scene-item type, no handle or object-row registration, and no source gesture routing.
- [Root inherits a node's GUI identity or prior document collapse] -> separate GUI ID scope and document-specific identity.
- [Search hides the root or preserves a previously collapsed container] -> always render the Scene row and expand once when a nonempty filter changes.
- [Reparenting under a collapsed scene hides the moved objects] -> reuse GUI hover expansion; it is intentional tree navigation, independent of drag start/end.
- [Tests miss the original jump] -> compare scene and object bounds before, during and after real drag input, not only domain outcomes.

## Migration Plan

No data migration. Rebuild Editor and tests; existing scenes retain their IDs, parent/local transforms and serialization versions. Reverting the presentation/controller-routing change restores the prior UI without touching assets. Leave the OpenSpec change active after verification and audit.

## Open Questions

None for this scope. Scene settings entry and multi-scene display can be designed separately when required.
