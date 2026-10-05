## Context

EditorStructure.cpp implements a complete Outliner reparent gesture but all mutable state lives on FEditorPlugin: gesture, nested-optional drop, expansion set and serial. Input policy, document reset and acceptance exercises inspect those fields directly. SceneEditing already owns the shared document, selection, history and atomic KeepWorld operation.

## Goals / Non-Goals

**Goals:** one private controller owns the gesture and every end/reset path; retain selection/click semantics, atomic commit, errors and external identifiers; use named drop alternatives and read-only observations.

**Non-Goals:** moving providers, documents, viewport or placement ownership; new domain services, callback buses or a general mutable frame context; feature changes.

## Decisions

- FEditorReparentController owns the optional gesture, token serial, pending root/node delivery and one-shot parent expansion set. FEditorPlugin delegates and holds no parallel gesture/drop state. Root/node/no delivery are a variant of named alternatives.
- Operations receive the existing FGui and FSceneEditDocument only for the synchronous call. The controller reads selection and target revision directly from their owner, uses PrepareReparent for feedback and ReparentSceneNodes for commit, and never stores references or callbacks.
- A private narrow IEditorReparentActions interface coordinates existing Inspector completion, selection/click behavior, document busy refresh and viewport input cancellation. Existing plugin methods implement it; a single new drag-start operation groups the existing viewport/camera reset. Policy decisions are passed as explicit permission values. This avoids a controller depending on the entire plugin or capturing its lifetime in callbacks.
- Feedback/results return optional error text and root-widget bounds to the plugin, which retains presentation and acceptance reporting. Existing acceptance exercises use a const gesture query; production policy uses HasGesture/IsDragging and Outliner consumes expansion through an operation.
- Cancel clears only the current gesture/delivery and matching GUI payload. Reset also clears expansion on document/content replacement and shutdown, before document detach or GUI destruction. Repeated cancellation/reset is safe, including partial startup with no GUI.
- Opening a replacement scene resets the controller after save/dirty admission succeeds and before Scene Close/Load. A synchronous path-validation/load failure therefore cannot retain the old gesture or payload; this cleanup does not clear domain history before a successful document reset. Real Editor acceptance exercises failed replacement and successful recovery in addition to CPU reset/detach cases.

## Risks / Trade-offs

- [Changed ordering around selection observers or document busy state] → preserve the original ordering; test the existing group selection, Ctrl/keyboard, cancellation and atomic-history exercises.
- [Old document or deleted target] → compare document/revision/selection before continuation; reset before detach and retain domain validation on preview/commit.
- [Shutdown before GUI acquisition] → controller reset accepts an optional GUI pointer and releases its own state without dereferencing an unavailable provider.
- [A private coordination interface adds inheritance] → it is not registered or published as a service and adds no plugin dependency/lifecycle; the interface contains only synchronous gesture actions.
