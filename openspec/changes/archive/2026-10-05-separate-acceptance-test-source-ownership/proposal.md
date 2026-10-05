## Why

Editor acceptance scenarios and unit test entry points currently share the production Private directory. Build selection works, but source ownership is inferred from filenames, making production/test leakage harder to detect.

## What Changes

- Move acceptance implementation, scenario state and unit test sources into Editor-owned Tests directories.
- Retain the production acceptance driver contract, observations, report and controlled unavailable implementation.
- Select acceptance sources explicitly with BUILD_TESTING and verify both configurations by actual target sources and compiler inputs.
- Strengthen source-boundary regression checks without changing scenarios or editor behavior.

## Capabilities

### New Capabilities

- `editor-test-source-ownership`: Explicit source ownership and test-enabled/disabled acceptance selection.

### Modified Capabilities

None.

## Impact

Editor plugin CMake and private test sources, acceptance boundary tooling and source-layout documentation. Target names, CLI exercise flags, reports and production observation contracts stay stable.
