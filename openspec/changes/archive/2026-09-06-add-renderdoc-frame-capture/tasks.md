## 1. Build and module boundaries

- [x] 1.1 Add optional pinned RenderDoc header bootstrap and CMake/helper switches with disabled-build isolation.
- [x] 1.2 Add Runtime/Capture wrapper and Plugins/RenderDoc lifecycle modules with private vendor access.

## 2. Capture service and Viewer integration

- [x] 2.1 Implement early DLL discovery, API negotiation, safe lifetime, capture ownership and verified output handling.
- [x] 2.2 Implement exact-file replay UI opening and independent launch status.
- [x] 2.3 Add configuration/CLI controls and pre-device plugin startup; bracket complete frames and clean up errors.
- [x] 2.4 Add shared disabled-aware RDC controls, status and optional automatic opening for both experiments.

## 3. Validation and documentation

- [x] 3.1 Add meaningful service, widget input and capture acceptance tests, including failure/retry and runtime-disabled behavior.
- [x] 3.2 Verify real Triangle and Model RDC capture/replay and automatic opening with the installed RenderDoc.
- [x] 3.3 Run style/boundary/naming checks and Debug/Release acceptance with RenderDoc enabled; verify disabled build.
- [x] 3.4 Document setup, API/thread/lifetime contracts, controls, troubleshooting and actual validation evidence.

## 4. OpenSpec completion

- [x] 4.1 Validate completed implementation against specifications, sync the new capability, and archive the change.
