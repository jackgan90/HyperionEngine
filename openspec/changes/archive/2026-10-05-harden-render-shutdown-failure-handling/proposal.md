## Why

A persistent device idle failure is reproduced in an isolated process: explicit session Close reports its exception, then destructor retry throws again and invokes std::terminate. The cleanup contract needs an explicit observable outcome when safe native retirement remains impossible.

## What Changes

- Preserve explicit Close failures, retryability and successful idempotent cleanup.
- Contain exceptions at session/resource-service destructor boundaries and share a documented terminal failure policy.
- Report owner, stage, reason and process-exit outcome before an immediate failure exit, without claiming GPU completion or releasing retained native state during unwinding.
- Verify persistent idle/collection failures, transient retry and real plugin shutdown in isolated subprocesses.

## Capabilities

### New Capabilities

- `render-shutdown-failure-policy`: Observable explicit-close and unrecoverable destructor cleanup outcomes.

### Modified Capabilities

None.

## Impact

Renderer session/resource destructors, private terminal failure helper, graphics lifecycle tests and shutdown documentation. No render feature, resource admission, device-fence or normal lifecycle behavior changes.
