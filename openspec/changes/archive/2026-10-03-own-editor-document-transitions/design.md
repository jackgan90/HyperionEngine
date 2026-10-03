## Context

At baseline `684b54f`, EditorDocumentTransition exposes pending paths/candidate, a string close state and seven control flags. EditorDocument.cpp, EditorCloseService.cpp and EditorContentTransition.cpp independently change related subsets. GUI and automation already share host services, and ContentRootService performs final participant preflight. This is an ownership refactor; the audit did not establish a new runtime defect.

Existing regression evidence includes root A/B/A, same-root preservation, save/discard/cancel, root preparation failure, scene save failure, title-bar cancellation during a gated save, API close failure followed by GUI cancellation, and native close overlapping a root prompt.

## Goals / Non-Goals

**Goals:** centralize transition decisions; distinguish target from phase; expose no mutable transition state; preserve current status/schema, save lifetime, dirty/history and content retirement contracts; make decisions testable without a GUI.

**Non-Goals:** general-purpose state-machine framework, new Runtime or plugin lifecycle, new automation operations, replacement of ContentRootService, changed save/cancellation capabilities, broad FEditorPlugin decomposition, acceptance-driver extraction, or the deferred discovery/test-isolation issue.

## Decisions

1. **Use explicit targets and phases with bounded owned pending requests.** Targets are document discard, scene open, content root and application close. Root and close requests carry phases such as requested/preparing, awaiting decision/path, saving, ready and failed. A close request may coexist with a prepared root or pending open; discard priority remains close, root, open, document. This represents the existing overlap without inventing a generic stack or forcing mutually exclusive targets that would change GUI behavior. Close protocol text is a read-only projection of the close phase.
2. **Expose operations, not setters.** Queue/prepare, begin/admit/reject save, observe scene-save failure, advance from a named save-progress snapshot, confirm discard, consume a ready root commit, cancel and complete are owner operations. Root/open payloads and errors remain private. Read-only queries serve GUI availability and tests. Repeating a poll or completing admitted IO after cancellation cannot recreate an intent or duplicate an action.
3. **Keep effects in the host.** The host provides current scene/asset dirty, pending-save, pending-edit and save-dialog facts, then performs the requested saves, root preparation/commit, load or native-window close. The owner does not look up services, pump tasks, draw UI, mutate documents or cancel admitted IO. Root commit still re-prepares and uses ContentRootService; the pending root is consumed before participant busy checks, preserving the existing self-preflight behavior. A failed commit retains the existing controlled-failure handling.
4. **Separate modal presentation from intent.** A private typed hidden/requested/visible presentation state supports one-shot popup requests, including the existing root-save prompt that stays dismissible while IO runs. BeginModal receives a local boolean; title-bar dismissal calls the same cancel operation as buttons and automation. GUI code cannot write owner flags.
5. **Retain host/API boundaries.** Existing scene open and content root automation services retain validation, revisions and persistence. application.close.request/status still expose idle/saving/failed/closing under the same schema. Close request acceptance remains distinct from process termination. Existing import-draft guards, provider lifetime and shutdown draining remain in place.

### Transition rules

| Event | Owner result | Host effect |
| --- | --- | --- |
| Dirty open or native close | Retain target and request decision presentation | Show existing unsaved-change UI |
| New dirty work after close acceptance | Reconfirm close while preserving the existing closing status | Require the close decision again before native exit |
| Root queued/prepared | Requested -> preparing -> decision or ready | Canonicalize/validate candidate; preserve same-root handling |
| Save chosen | Saving, or awaiting scene path | Admit existing asset/scene saves; show Save As if required |
| Close save admitted over replacement | Retain close continuation and retire root/open intent | Later save failure returns only to the close decision |
| Save admission fails | Root returns to decision; close records failed status | Keep dirty documents and report existing error |
| Scene save fails asynchronously | Abandon pending root; retain close failure detail until saves settle | Show root error; preserve admitted asset work |
| Saves settle clean | Root ready or close ready | Revalidate/commit root, or request native close |
| Saves settle dirty | Root decision, or failed close decision | Preserve draft/history and permit retry/discard/cancel |
| Cancel button/title bar/API | Remove applicable pending intents and popup request | Admitted saves continue against their original documents |
| Confirm discard | Resolve existing priority; return one owned action | Reset/open, authorize root discard, or discard/close |
| Root change or accepted close | Clear obsolete replacement/presentation state | Existing document/resource retirement and shutdown |

## Risks / Trade-offs

- Overlapping close/root requests cannot be modeled as one exclusive target without changing behavior -> explicit bounded intents and fixed precedence, covered by owner and existing GUI tests.
- Modal dismissal and asynchronous completion occur in different ticks -> all cancellation removes continuation authority; deterministic tests complete saves after cancellation.
- Root commit invokes participant callbacks while executing the transition -> consume the pending intent before final commit, and verify busy/dirty rejection with the real service.
- A simple accessor-only rewrite would preserve distributed rules -> callers supply facts and effects; the owner alone selects phase/error/presentation outcomes. Review all Transition consumers and prohibit mutable access.
- Existing EditorApplication.h and EditorPanels.cpp exceed the file guideline -> only a host helper declaration and query substitutions are in scope; preserve unrelated structure and record this migration limitation. Substantive changed/new functions and new production files meet the 100/500-line guidance.

## Migration Plan

Record existing Debug behavior, implement the owner and focused state tests, migrate document/close/root adapters and read-only consumers, then run Debug/Release builds and targeted real GUI/CLI/MCP tests plus style/naming/boundary/spec checks. Use task-local discovery directories for this validation without changing the deferred discovery implementation. No schema or persisted data migration is needed. Leave this change active and uncommitted.

## Open Questions

None requiring user input; scope and compatibility are set by the selected audit item and current regression contracts.
