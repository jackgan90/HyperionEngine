## Why

Editor capture acceptance modes are validated at the CLI boundary but retained and compared as strings throughout the acceptance harness. Typed identity can remove repeated token-based control flow while preserving the existing command-line and test behavior.

## What Changes

- Represent an optional capture exercise with an Editor-owned enum.
- Define the four CLI/log names once and convert only at input and presentation boundaries.
- Use typed values for input routing, screenshot timing, timeout activation and GUI persistence policy.
- Preserve CLI rejection order/messages, isolated preference requirements, test-build gating and every exercise sequence.

## Capabilities

### New Capabilities
- `typed-editor-capture-exercises`: Typed private capture acceptance modes with compatible CLI and execution behavior.

### Modified Capabilities
None.

## Impact

Editor private options and acceptance harness, focused options tests and test target registration. No new user-facing feature, automation operation, reflection/persistence schema, plugin dependency or capture algorithm. Other exercise options and document-close status remain outside this stage.
