## Why

Editor can open and save scenes but has no explicit New Scene or Close Scene command. Both commands must protect unsaved work using the existing save/discard/cancel flow while keeping the application and independent asset documents usable.

## What Changes

- Add File > New Scene and Close Scene, with a distinct closed-document presentation and a zero-object untitled scene.
- Extend typed host transitions with save-before-new/close, explicit discard, cancellation, asynchronous failure recovery and stale-document protection.
- Route untitled Save and Ctrl+S through Save As and preserve empty-scene persistence.
- Expose discoverable `scene.new` and `scene.close` operations using reflected contracts and the same services as GUI commands; preserve `scene.open` compatibility.
- Keep scene retirement, auxiliary-window modality and asset editing valid with no open scene.

## Capabilities

### New Capabilities

- `editor-scene-lifecycle`: Explicit new/close scene operations, unsaved-change decisions, empty-scene persistence and closed-document behavior.

### Modified Capabilities

None. Existing scene opening, application exit and content-root contracts remain compatible.

## Impact

Runtime/SceneEditing host request/response contracts; Editor document transition, lifecycle, menus, interaction and render paths; Automation scene bindings; focused unit and real Editor acceptance coverage; Editor and Automation documentation. No new dependency, transport branch, application lifecycle, asset format or implicit Game mount is introduced. The change remains unarchived and uncommitted after validation.
