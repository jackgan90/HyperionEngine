## 1. Runtime framing

- [x] 1.1 Extract validated bounds-to-camera fitting while preserving existing whole-scene behavior.
- [x] 1.2 Add selected-subtree bounds resolution with affine model bounds, overlap de-duplication, readiness errors and non-geometric fallbacks.
- [x] 1.3 Add renderer regression coverage for union centers, hierarchy, transforms, aspect/clip fitting and invalid-input atomicity.

## 2. Shared viewport and automation

- [x] 2.1 Extend the UI-independent viewport contract and implement document/revision/readiness/preview validation with atomic camera publication.
- [x] 2.2 Register reflected `view.frame_selection` with provider availability and unchanged existing operation contracts.
- [x] 2.3 Validate real discovery/describe/invocation, cross-connection state, stale/empty/preview rejection and absent providers.

## 3. Editor interaction

- [x] 3.1 Add engine F-key and SDL/ImGui mappings, preserving existing key values.
- [x] 3.2 Route F through scene-panel focus and full-frame text/popup/gesture guards; preserve Home and consistent viewport overlays.
- [x] 3.3 Remove the main toolbar Frame Scene button and update navigation guidance.
- [x] 3.4 Add Editor GUI acceptance for viewport/Outliner selection, multi-selection, repeat/modifier/text/popup/focus/preview isolation and document invariance.

## 4. Validation and delivery

- [x] 4.1 Update Editor and automation capability documentation with final semantics.
- [x] 4.2 Build Debug and Release and pass targeted renderer, GUI, Editor, automation and plugin absence/shutdown regression tests.
- [x] 4.3 Pass style/naming, module-boundary, diff and strict OpenSpec checks; record validation and leave this change active without archiving or committing.

## 5. Independent audit corrections

- [x] 5.1 Confirm and repair FS-01 full-scene clipping and FS-02 shared-provider interaction admission.
- [x] 5.2 Pass Debug/Release GUI and automation regressions covering F-to-Home clipping, active RMB and ordinary popups.
- [x] 5.3 Complete targeted independent re-review and record the final audited snapshot without archive or commit.
