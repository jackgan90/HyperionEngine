## 1. Lifecycle and content

- [x] 1.1 Add factual startup, cleanup and content-root transition/recovery diagnostics.
- [x] 1.2 Add terminal import, draft and document-save diagnostics in shared services.

## 2. Renderer diagnostics

- [x] 2.1 Expose scene load, dependent-resource and refresh failures without repeated polling messages.
- [x] 2.2 Report D3D12 warnings/errors once per distinct diagnostic and retain validation counters; expose actual shader compilation diagnostics while keeping cache hits quiet.

## 3. Validation and guidance

- [x] 3.1 Add targeted regressions for context, severity, terminal-event uniqueness and native-message deduplication.
- [x] 3.2 Document logging boundaries and shared automation access.
- [x] 3.3 Build affected configurations, run targeted tests and style/boundary/OpenSpec checks; initially deliver unarchived and uncommitted (subsequent archive and commit authorized on 2026-09-30).
