## 1. Shared selection

- [x] 1.1 Add atomic validated batch selection and reflected select-all summary, preserving primary and removing only the explicit-selection 128 limit.
- [x] 1.2 Register scene.selection.select_all through automation-scene and test discovery, schema, call, large selections, atomic rejection and absence.

## 2. Editor input

- [x] 2.1 Expose Shift in Gui pointer state and capture viewport Ctrl-or-Shift toggle at press.
- [x] 2.2 Add displayed-order Outliner range state and integrate release/drag/keyboard routing with safe anchor lifecycle.
- [x] 2.3 Route focus-aware Ctrl+A with text, popup, navigation, gesture and repeat guards.

## 3. Verification and documentation

- [x] 3.1 Add pure range-state and Gui modifier tests plus real Editor selection-shortcut acceptance and GUI/automation equivalence checks.
- [x] 3.2 Update Editor/Automation capability documentation and validate the active OpenSpec change.
- [x] 3.3 Build Debug/Release, run affected regressions and style/naming/boundary checks, record evidence, and leave the change active without committing.
