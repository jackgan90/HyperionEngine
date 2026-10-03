## Why

Editor acceptance implementations are already selected by BUILD_TESTING, but production still declares scenario methods, stores test snapshots and widget bounds, and branches on individual cases. This makes test changes affect the production class and drawing/frame behavior; the audit identified an ownership problem rather than a reproduced user-facing failure.

## What Changes

- Move scenario methods, steps, assertions, captured widget data and fixture snapshots into a private acceptance implementation behind the existing driver.
- Replace case-specific production branches with typed UI observations and focused lifecycle, input, render and capture hooks. Keep real editor operations in their existing production owners.
- Preserve existing acceptance CLI flags, report keys/types, screenshots, timing and input-window selection. Normal finite-frame, report, benchmark and interactive behavior remain available without acceptance implementation.
- Verify both BUILD_TESTING-enabled and disabled compilation/linking and runtime behavior, plus the existing GUI and automation regressions.
- Keep the change active and uncommitted after implementation and verification.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-application-consolidation`: Complete the separation of acceptance scenarios/state from the production Editor class, while retaining supported observation points and test-disabled behavior.

## Impact

Editor private driver, scenario sources, production GUI/frame/report consumers, internal startup options, CMake selection and ownership documentation. No new Runtime module, plugin lifecycle, public automation operation, transport branch, persisted schema or asset migration is introduced. The previous document-transition owner and shared GUI/automation services remain authoritative. Discovery/test-isolation repair, unrelated panels-as-plugins work and broader editor decomposition remain outside this change.
