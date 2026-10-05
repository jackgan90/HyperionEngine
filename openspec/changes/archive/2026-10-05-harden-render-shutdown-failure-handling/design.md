## Context

The graphics plugin Stop catches session Close errors and resets the session. Both session and resource-service destructors call Close again. A persistent WaitIdle fault reproduced in an isolated process reaches std::terminate from that retry, including the real graphics Stop chain. Native retirement cannot safely finish when completion is unknown.

## Goals / Non-Goals

Define an observable failure outcome at destructor boundaries while retaining explicit Close propagation, transient retry, normal idempotency and the existing task/GPU retirement sequence. Do not replace the renderer lifecycle, fabricate completion or dispose of retained GPU objects after a failed wait.

## Decisions

### Explicit Close remains retryable

Keep current Close state transitions and exception propagation. A one-time wait failure can be retried successfully; successful close remains idempotent. Do not set native-closed/session-closed flags merely to avoid destructor errors.

### Terminal destructor policy

Each destructor catches any Close exception and calls one private shared terminal helper. It records owner, destructor-close stage, original exception reason, immediate-process-exit outcome and EXIT_FAILURE. It then uses std::_Exit(EXIT_FAILURE), which performs no C++ member/atexit unwinding. Pending native state is left for process/driver teardown instead of releasing objects whose GPU completion is unknown. No destructor exception escapes to std::terminate.

The normal engine logger records and flushes the message; if logging itself throws, a minimal stderr fallback records the same facts. Flush C streams before immediate exit. The helper never reports successful GPU/native retirement. This explicit fatal policy is chosen because suppressing the exception, falsely closing the owner, or continuing member destruction could release still-used native objects. Quarantining work until a recoverable device replacement would require a wider device-lifecycle design and is outside this change.

### Process-isolated regression coverage

Use the public-contract fake provider with persistent idle/collection fault hooks and call counters. Test direct session/resource destruction and the real graphics plugin Stop path in separate processes. Install a test terminate sentinel and native owner destruction sentinels, assert the failure marker and exit code, and assert those sentinels are not reached. Existing transient retry, shutdown wait reporting and real D3D12 fence/resource retirement tests remain required.

## Risks / Trade-offs

- Immediate process exit does not run unrelated application cleanup → reserve it for destructor cleanup failure where safe retirement cannot be established; document the outcome.
- Log callbacks can throw → contain logging failures and retain stderr fallback.
- Fault tests could produce false success without retained state → use destruction sentinels and original exception/call-count checks; retain actual GPU retirement regressions.

## Migration Plan

Add the shared helper and destructor containment, document the failure outcome, verify process-isolated faults and existing normal/transient shutdown paths, then independently audit. No persistent asset/config migration is needed.
