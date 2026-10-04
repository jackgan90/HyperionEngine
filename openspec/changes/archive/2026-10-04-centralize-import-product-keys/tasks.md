## 1. Contract and implementation

- [x] 1.1 Add fixed historical OutputIds/publication fixtures and verify them against the unchanged production implementation.
- [x] 1.2 Centralize shared-image format/parsing, publication key inputs and persisted constants; migrate production callers without policy changes.
- [x] 1.3 Add private compatibility edge tests and document the two identity layers and opaque extension boundary.

## 2. Validation

- [x] 2.1 Build affected targets and run publication, sharing, model conversion and CLI/automation import regressions.
- [x] 2.2 Complete style, naming, boundary and strict OpenSpec checks; manually inspect semantic ownership and function/file sizes.

## 3. Independent review and acceptance

- [x] 3.1 Freeze the change and run an independent quality-audit reviewer; verify findings and minimally repair/review any confirmed issues.
- [x] 3.2 Record the reviewed scope and validation limits, then present the diff for user acceptance before archive or commit.
