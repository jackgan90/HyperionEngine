## 1. Image contract and consumers

- [x] 1.1 Introduce the data-only module, preserve Assets re-exports and migrate RHI/Gui/Renderer includes with direct dependencies.
- [x] 1.2 Protect RHI's dependency closure, add adversarial coverage and add a public image consumer; update current ownership documentation.

## 2. Validation and audit

- [x] 2.1 Build affected Debug/Release and verify graph, public consumer, codecs, texture and GPU image/output regressions; check style and OpenSpec.
- [x] 2.2 Independently audit the entire change with quality-audit, verify/fix findings and re-review confirmed repairs.
