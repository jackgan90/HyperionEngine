## 1. Process logging

- [x] 1.1 Add Debug and thread-safe disk-backed history with bounded reads and current-run isolation.
- [x] 1.2 Add scoped Platform stdout/stderr capture with original-output forwarding and orderly tail draining.
- [x] 1.3 Integrate entrypoint history and GUI subsystem with nonblocking automated failure diagnostics.

## 2. Editor presentation

- [x] 2.1 Extend Gui bottom-tab placement and preserve existing/custom layouts.
- [x] 2.2 Add raw colored virtual log rows, tail following and Window > Log toggling.

## 3. Automation and documentation

- [x] 3.1 Register reflected application.log.read using the shared history and scoped lifecycle.
- [x] 3.2 Document log behavior, history boundaries, startup behavior and automation coverage.

## 4. Validation

- [x] 4.1 Cover concurrent history, paging, severity, raw streams, long/multiline text and shutdown tails.
- [x] 4.2 Cover fresh/old/custom/reset docking, colors, scrolling and actual Editor menu behavior.
- [x] 4.3 Verify automation discovery/schema/calls, absence, console subsystem, redirection and startup failures.
- [x] 4.4 Run affected builds, regressions, style/boundary checks and strict OpenSpec validation; leave change unarchived and files uncommitted.
- [x] 4.5 Reproduce and repair audit finding LOG-001; cover split UTF-8 characters at the native pipe fragment boundary.
