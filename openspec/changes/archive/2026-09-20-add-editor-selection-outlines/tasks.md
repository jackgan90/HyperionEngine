## 1. Target ownership

- [x] 1.1 Add immutable grouped outline requests, SceneBridge resolution and direct Render primitive subset collection with publication/generation safeguards.

## 2. Renderer

- [x] 2.1 Implement cached standard PBR/custom silhouette materials preserving alpha coverage and local overrides.
- [x] 2.2 Implement union/per-object mask and outline passes, post-tonemap composition, owned resources, empty/reset/resize behavior and optional supersampling.

## 3. Editor

- [x] 3.1 Activate outlines only in Editor, submit existing selection and expose immediate overlap-mode/quality options with Union default.
- [x] 3.2 Add a repeatable multi-target comparison exercise without changing normal selection gestures.

## 4. Verification and delivery

- [x] 4.1 Add and run GPU/ownership regressions for silhouette behavior, both overlap policies, alpha coverage, frame changes and lifecycle paths.
- [x] 4.2 Build and run affected editor/render/plugin regressions, inspect comparison images and verify GPU validation.
- [x] 4.3 Update project documentation, run style/boundary/naming/OpenSpec checks and record validation; leave changes uncommitted.
