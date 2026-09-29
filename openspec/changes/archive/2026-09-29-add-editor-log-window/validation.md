# Validation

Validated on 2026-09-29. This change remains active and uncommitted as requested.

## Builds

- Debug: Editor, Automation CLI, Core/history, process-output, log-automation and Gui tests built and linked.
- Release: the same targets built and linked with the existing Triangle/DebugUI-disabled configuration.
- RelWithDebInfo: Editor built and linked; process-output startup smoke and real Editor Log/attached JSONL/MCP acceptance passed.
- PE subsystem assertions verify Editor and the process-output child use Windows GUI subsystem (2). The entry retains CRT `main` argument handling.

## Regression results

Debug targeted suite: 10/10 passed:
`automation_transport`, `core`, `log_history`, `log_automation`, `process_output`, `editor_log`, `plugin_applications`, `gui_docking`, `gui_log`, `editor_acceptance`.

Release targeted suite: 9/9 passed: the same set excluding the broader existing `editor_acceptance` suite.

After restoring the standalone CLI's original stderr log pattern, the final Debug `automation_transport` rerun passed. Release tests and RelWithDebInfo smoke/Editor acceptance used that final source.

Coverage includes:

- Early Debug history, concurrent producers, cursor order, invalid reads, UTF-8 long lines, multiline messages and current-run isolation.
- CRT/native stdout/stderr, exact-once structured forwarding, partial writes, final tails, redirected failures, detached/no-console launch and non-inheritable capture handles.
- New and old docking layouts, preserved floating settings, reset, severity vertex colors, visible-row-only reads and scroll retention with growing output.
- Real Window > Log open/close/reopen input, output while closed, startup replay, saved rendered PNG and attached JSONL/MCP discovery/description/calls.
- Missing history provider, disabled Editor/Gui/Graphics paths and existing automation stdio protocol isolation.

Visual inspection of `out/log-tests/editor-0sik0ka7/Log.png` confirmed the bottom Content Browser/Log tab group, white Info/Debug, yellow multiline Warning, red Error/stderr and original `/Game` text. Final RelWithDebInfo acceptance produced `out/log-tests/editor-ipc5qkme/Log.png`.

## Static checks

- `python tools/CheckStyle.py`: passed filenames/include casing and formatting.
- Changed C++ translation units: semantic naming and boolean-prefix checks passed (18 units).
- `python tools/CheckBoundaries.py`: passed.
- `git diff --check`: passed.
- `openspec validate --all --strict`: 100/100 passed, including this change.

## Quality audit follow-up

The independent audit examined the 44-file working-tree snapshot against baseline `8046ddb54bfe60a57fd54ce8c6a2269e958b1b05` and confirmed one P2 finding, LOG-001. When a pipe read ended exactly at 8192 bytes inside a UTF-8 character, capture emitted two invalid UTF-8 records; GUI text and strict JSON serialization of `application.log.read` were affected. The main agent independently reproduced the defect and accepted the finding. No other confirmed findings were reported.

The repair retains an incomplete final code point in the pending pipe buffer while delivering the complete prefix. External forwarding and the history/automation contracts remain unchanged. The regression uses actual native pipe writes and synchronizes on receipt of the prefix before writing the continuation. It covers every incomplete prefix of 2-, 3- and 4-byte characters, both newline and EOF draining, independent UTF-8 validity of every history record and byte-for-byte reconstruction.

The new regression failed on the pre-fix code. After repair, the original independent probe produced valid 8191-byte and 3-byte records and strict JSON serialization succeeded. Debug, Release and RelWithDebInfo Editor/process-output builds passed. Final targeted tests passed: Debug 6/6 (`log_history`, `log_automation`, `process_output`, `editor_log`, `gui_docking`, `gui_log`), Release 2/2 (`process_output`, `editor_log`), and RelWithDebInfo 1/1 (`process_output`). Source style, module boundaries and semantic naming checks passed. Initial audit/probe evidence and the follow-up snapshot are retained under `out/EditorLogAudit`.

The original independent reviewer rechecked the fix and closed LOG-001 with no additional confirmed findings. Their separate Debug pipe run passed all 12 scenarios and verified exact forwarding of all 98330 input bytes; all 44 reviewed file hashes matched the follow-up snapshot. The report is `out/EditorLogAudit/FollowupReview.md`. Final OpenSpec strict validation passed 100/100 and `git diff --check` passed. This paragraph records the completed review after the frozen review ended; no source changed afterward.

## Delivery boundaries

No archive, spec synchronization, staging, commit or push was performed. Logging starts at entrypoint initialization, excludes other processes and OS debug streams, and drains on orderly shutdown. Raw streams have fallback severity/source metadata; short unterminated fragments become records when completed or drained. Current-run text is disk-backed with a resident offset index; no cache-driven history eviction occurs.
