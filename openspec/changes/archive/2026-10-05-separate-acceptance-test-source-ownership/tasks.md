## 1. Source ownership

- [x] 1.1 Move Editor-owned acceptance and unit test sources, update explicit CMake selection and local includes, and document the production contract.
- [x] 1.2 Strengthen ownership/compiled-source boundary checks with positive and negative fixtures.

## 2. Verification and audit

- [x] 2.1 Build Debug/Release with testing enabled and disabled; run affected unit, actual acceptance, normal startup and unavailable-mode regressions.
- [x] 2.2 Validate style, dependency boundaries and strict OpenSpec; independently audit and repair/re-review confirmed findings.

## 3. Review follow-up

- [x] 3.1 Derive private/test header ownership from configured consuming targets, including module-local Tests headers, while preserving public, private and vendor isolation.
- [x] 3.2 Add positive/negative header-consumer fixtures, check configured ON/OFF graphs and independently audit the correction.
