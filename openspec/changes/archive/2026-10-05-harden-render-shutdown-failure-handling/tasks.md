## 1. Failure policy

- [x] 1.1 Reproduce persistent idle failure in isolated direct-session and real graphics Stop chains and preserve evidence.
- [x] 1.2 Add shared terminal failure handling at destructor boundaries and document explicit Close/transient-retry/unsafe-retirement behavior.
- [x] 1.3 Cover persistent idle/collection, retained-owner sentinels, logging fallback and transient/normal lifecycle regressions.

## 2. Verification and audit

- [x] 2.1 Build Debug/Release and run shutdown/resource/fence/plugin failure tests; validate style, boundaries and strict OpenSpec.
- [x] 2.2 Independently audit and repair/re-review confirmed findings before final acceptance.
