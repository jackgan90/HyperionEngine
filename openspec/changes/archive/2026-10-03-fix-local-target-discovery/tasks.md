## 1. Discovery contracts and exact lookup

- [x] 1.1 Add a regression demonstrating exact lookup failure under enumeration saturation.
- [x] 1.2 Add shared exact-instance discovery and typed enumeration metadata; route probe/connect through exact lookup.

## 2. Registration ownership and retirement

- [x] 2.1 Add private versioned records with legacy read compatibility and native three-state process identity checks.
- [x] 2.2 Filter proven-stale records and perform bounded safe cleanup at publication; preserve unknown and concurrently changed records.
- [x] 2.3 Cover invalid/bounded input, both list limits, legacy/unknown/alive/dead/PID-reused ownership and safe deletion with deterministic and native tests.

## 3. Acceptance isolation and integration

- [x] 3.1 Isolate acceptance discovery per run across application/CLI/MCP children and C++ lifecycle publishers, preserving explicit failure overrides.
- [x] 3.2 Verify real saturated Editor probe/connect and CLI attachment, absence/disablement/failure/shutdown behavior and repeat-run isolation.

## 4. Documentation and validation

- [x] 4.1 Document discovery limits/metadata, private format compatibility and acceptance environment behavior.
- [x] 4.2 Complete relevant Debug/Release builds and tests, style/boundary/naming checks and manual semantic/size review; record evidence and validate OpenSpec without archiving or committing.
