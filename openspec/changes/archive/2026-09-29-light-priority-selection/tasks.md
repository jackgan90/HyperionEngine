## 1. Scene contracts

- [x] 1.1 Add reflected priorities and shared deterministic lighting resolution with candidate/tie tests.
- [x] 1.2 Remove authored light selections and update scene persistence, authoring, fixtures and default-sky behavior.

## 2. Rendering

- [x] 2.1 Publish immutable resolved lighting and use the priority shadow/sky source consistently.
- [x] 2.2 Add reusable directional-list resources and shared additive lighting in Forward, Deferred and transparency.

## 3. Editor and automation

- [x] 3.1 Replace action blocks with Priority fields and contextual priority/sky asset diagnostics.
- [x] 3.2 Remove obsolete main-light controls and expose shared resolved diagnostics with component authoring parity.

## 4. Validation and documentation

- [x] 4.1 Update meaningful CPU, automation, editor and GPU regressions for priority changes, failures and multi-light behavior.
- [x] 4.2 Update current docs, run style/boundary checks, builds and affected tests, record results; leave change active and uncommitted.

## 5. Acceptance refinement

- [x] 5.1 Show the effective sky tooltip status in green and the overridden status in red using the winner's display name; suggest increasing Priority without exposing persistent IDs, and validate the affected Editor/GUI paths.
- [x] 5.2 Give sky and directional highest-priority warnings independent hover explanations, apply colored status and actionable Priority advice to directional shadows, and validate the affected GUI/Editor paths without exposing internal IDs.

## 6. Quality audit

- [x] 6.1 Fix LP-01: retire Deferred directional buffers and bindings with scene-input generations; reproduce the leak, verify bounded live resources and retained frames, and obtain independent re-review.
