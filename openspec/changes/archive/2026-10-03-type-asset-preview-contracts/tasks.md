## 1. Compatibility baseline

- [x] 1.1 Capture the existing preview discovery/schema and request/result representations and run the relevant original asset workspace/editor regressions.

## 2. Explicit preview contracts

- [x] 2.1 Add typed shape/channel descriptors, strict numeric mappings and fixed-expectation contract tests for AssetEditing.
- [x] 2.2 Migrate Editor state, shared service, preparation and GUI to the typed options; preserve wire fields, selection order and invalidation.
- [x] 2.3 Replace positional sky products and format captions with named storage/explicit mappings and focused association tests.

## 3. Behavioral validation

- [x] 3.1 Add reordered/relabelled GUI mapping checks and verify all channels, shapes, invalid values and preview-only history behavior through the shared workspace.
- [x] 3.2 Verify unchanged discovery/schema and real attached automation round trips, including invalid and unsupported settings.
- [x] 3.3 Build Debug/Release and pass focused tests plus affected editor, workspace and automation regressions.
- [x] 3.4 Update documentation and verification evidence; pass OpenSpec validation, formatting, semantic naming, dependency and diff checks; leave the change active and uncommitted.

## 4. Quality-audit follow-up

- [x] 4.1 Include the preview contract executable in hyperion_check dependencies and verify both generated build graphs and the contract tests; obtain targeted independent re-review.
