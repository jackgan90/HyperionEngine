## Context

Migration added two structural actions and a scene-role submenu to Edit. Acceptance requests their removal. The shared document implementation and Automation operations are independently useful; Details hierarchy also uses the same local node chooser as the unwanted submenu.

## Goals / Non-Goals

Goals: remove the three menu entries and unused drawing methods; accurately document current GUI and Automation availability.

Non-goals: new Outliner/viewport interactions, removal of shared transactions or Automation operations, changes to existing role selection or undo/redo, Git commits.

## Decisions

Delete the two Edit menu call sites, private declarations and drawing methods. Retain the local chooser for Details hierarchy. Removing only the calls would leave dead code; deleting the whole file would also remove the independently used hierarchy controls.

Preserve scene-role controls and shared editing services. Record duplication/keep-children GUI integration as deferred instead of moving the removed actions to another surface in this change.

## Risks / Trade-offs

- Menu positions change -> run existing Editor acceptance and multi-selection tests against the rebuilt executable.
- Shared hierarchy helper could be removed accidentally -> compile the Editor and check remaining call sites and shared Automation scene tests.
- Prior migration remains uncommitted -> limit edits to the requested menu/documentation follow-up and archive it separately.
