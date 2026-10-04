## Context

At ba3f17b, FEditorDocumentTransition owns typed phases and derives CloseStatus for EditorCloseService's public application.close response. Tests instead compare that lossy string: idle represents Idle/AwaitingDecision/AwaitingSavePath and closing represents Ready/Reconfirming. Fifteen native assertions and two GUI acceptance checks can use the existing phase query directly.

## Goals / Non-Goals

**Goals:** Verify precise close decisions, pending save stages, failure, cancellation and completion through the owning type; retain a focused public text-compatibility test and existing effects/error checks.

**Non-Goals:** Change the transition implementation, add another enum/state store, expose native phases in automation, alter errors/schema/persistence/lifecycle, or migrate unrelated preview/save diagnostics. This is a bounded first part of Editor observation work.

## Decisions

1. Replace semantic CloseStatus assertions with exact EEditorTransitionPhase comparisons. Determine each expected phase from the operation sequence, not a mechanical text-to-enum mapping: a fresh dirty close is AwaitingDecision; a renewed dirty request after Ready is Reconfirming. Cancellation is Idle and successful completion is Ready.
2. Assert AwaitingSavePath in existing cancellation cases, and verify the owner stays Saving after recording a scene failure until the continuation advances with dirty state. Preserve pending-intent, decision visibility, target precedence, error forwarding and side-effect checks.
3. Put CloseStatus expectations in one focused projection test driven by public domain operations. Cover the reachable close phases and fixed existing status tokens. Keep synthetic caller-provided error strings in semantic tests because those assert lossless error forwarding, not formatted UI prose.
4. Replace two GUI close checks in EditorContentAcceptance with Failed/Idle comparisons, retaining real failed persistence, permission restoration, dirty state and window/decision assertions. Existing Python automation checks remain string-based because they intentionally validate the wire boundary.
5. No product capability is added and no production file changes; no adapter, schema update, provider-lifecycle matrix or production build change is needed. Validate the affected native and real GUI paths plus existing close automation regression coverage.

## Risks / Trade-offs

- Mapping idle/closing mechanically could select the wrong native phase → trace each transition and explicitly cover the phases merged by text.
- Removing all string checks could lose wire compatibility coverage → keep one fixed projection test and existing external automation assertions.
- Scope could expand into Editor ownership refactors → only change the two acceptance/test consumers and OpenSpec; defer preview/save observation work.

## Migration Plan

No data migration. Build, run targeted regressions and style checks, freeze the diff for a fresh independent quality-audit review, resolve verified findings and present for user acceptance before archive/commit.
