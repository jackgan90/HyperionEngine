## 1. Shared scene clipboard

- [x] 1.1 Add immutable component snapshot/reference contracts and document-scoped clipboard capture, validation, budgets and numbered naming.
- [x] 1.2 Add atomic creation-batch history with undo/redo, ordered selection and external reference remapping.
- [x] 1.3 Integrate resource preparation and document/content lifetime invalidation.

## 2. Platform and Editor

- [x] 2.1 Add typed platform clipboard with real Windows custom formats, text interoperability and controlled errors.
- [x] 2.2 Inject the clipboard provider and route scene Ctrl+C/Ctrl+V with focus, text ownership and gesture guards.

## 3. Automation and documentation

- [x] 3.1 Register reflected copy/info/paste operations with shared domain behavior, bounded results and unavailable contracts.
- [x] 3.2 Document user behavior, supported scope and automation coverage.

## 4. Verification

- [x] 4.1 Add domain/history/resource and automation regressions for snapshots, hierarchy, failures, names and lifecycle.
- [x] 4.2 Add real platform clipboard and Editor input/GUI acceptance coverage including text replacement.
- [x] 4.3 Build affected targets, run targeted tests, style/naming/boundary checks and strict OpenSpec validation; record evidence without archiving or committing.

## 5. Placement acceptance follow-up

- [x] 5.1 Reproduce direct copy/paste after a GUI placement drop, hand successful drops' keyboard focus to the viewport, and verify all placeable types plus existing clipboard isolation.

## 6. Independent audit

- [x] 6.1 Reproduce and fix CLIP-01: refresh copied model resources after all live consumers were removed; verify failure isolation and obtain independent re-review.
