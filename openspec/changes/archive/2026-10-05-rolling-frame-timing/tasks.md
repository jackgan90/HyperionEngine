## 1. Shared statistics

- [x] 1.1 Implement the bounded every-frame timing window and reflected statistics value with deterministic coverage, percentile and invalid-input behavior.
- [x] 1.2 Add the statistics to shared diagnostics and collect measured Editor intervals independently of HUD/profiling visibility.

## 2. Presentation and contract

- [x] 2.1 Update the overview HUD with explicit average/window labels and Last/P95/Max/long-frame count.
- [x] 2.2 Document the shared diagnostics fields, measurement boundary, window and threshold semantics.

## 3. Verification

- [x] 3.1 Add deterministic statistics/reflection regression coverage and validate automation discovery/invocation and HUD isolation.
- [x] 3.2 Build Visual Studio RelWithDebInfo; run style/boundary and targeted render-control tests; verify the Sponza overview visually and through automation.
