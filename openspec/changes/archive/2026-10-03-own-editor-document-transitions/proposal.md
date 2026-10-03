## Why

FEditorDocumentTransition exists, but callers still directly coordinate pending roots, pending opens, close status and save/discard flags across several Editor files. This leaves cancellation, save failure and overlapping close/root decisions dependent on combinations that have no single owner.

## What Changes

- Make the private Editor transition owner enforce request, decision, save, cancellation and completion rules through semantic operations and read-only queries.
- Model transition targets and progress phases explicitly, including the existing close-over-root-over-open discard precedence and a close request that overlaps a pending root decision.
- Keep actual document saves, root preparation/commit, window actions and GUI presentation in their existing hosts; ContentRootService remains the final dirty/busy/generation authority.
- Preserve automation operation IDs, schemas, close status strings, admitted save completion and existing GUI behavior. Add focused transition tests and run real Editor/automation regression coverage.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-application-consolidation`: Require the private transition owner to encapsulate target/phase state and enforce cancellation, failure recovery and overlap rules.

## Impact

EditorDocumentTransition, document/save/close/root host adapters and their read-only consumers; focused Editor tests and current ownership documentation. No new Runtime module, application lifecycle, transport branch or user-facing capability is introduced. Acceptance tests change only as needed to consume the new transition API; broader acceptance isolation, discovery/test-isolation repair, cache work and reflection changes remain out of scope. Complete implementation and testing without archiving or committing.
