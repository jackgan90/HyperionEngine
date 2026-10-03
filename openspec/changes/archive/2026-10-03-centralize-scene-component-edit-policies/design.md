## Context

SceneEditing rejects model binding edits through a literal field list. Editor single and selection inspectors apply reflected drafts to detached node candidates and commit through the same document, but do not run the component field rule. Canonical inspector hints also appear in published wire schemas and must remain compatible.

## Goals / Non-Goals

Goals: one domain-owned immutable-field decision, shared candidate checks, and contextual presentation consuming that decision for both single and selection editing.

Non-goals: changing serialized reflection definitions, granting new edit rights, adding component kinds, modifying generic history semantics, resource rebinding, or changing Editor transitions.

## Decisions

1. SceneEditing owns a focused component edit policy. Model binding restrictions match reflected C++ member associations against actual member pointers; model-source components remain entirely immutable. Unknown component types retain their existing editable behavior. No string whitelist or presentation-derived authorization remains in the domain. This uses the existing typed association directly, avoiding repeated descriptor validation during Inspector drawing.
2. Shared candidate validation compares original and proposed persistent immutable members. Automation invokes it before applying values; both GUI draft paths invoke it on their detached candidates before queuing document edits. Keep model/resource validation and the document transaction as separate owners. Do not put the restriction in generic CommitEdits, which also serves other authoring operations and restoration.
3. GUI presentation consults the same policy through its existing single-record presentation callback and an equivalent optional callback for selection inspection. Existing inspector hints remain presentation metadata for schema compatibility; they never authorize mutation. Contextual display can impose extra presentation restrictions without weakening domain checks.
4. Preserve continuous-edit interaction IDs and per-target candidates. Add CPU/automation rejection, unchanged-source acceptance, single/selection GUI-equivalent candidate tests and discovery/schema coverage. Existing Editor acceptance verifies real GUI behavior.

## Risks / Trade-offs

- Display projections can have different member names: policy checks use canonical candidates and types, while contextual hints apply only to recognized canonical members. Do not infer canonical identity from labels.
- Over-broad document enforcement could prevent history or prepared placement: keep the check at component editing admission, not all document mutations.
- Presentation hints are retained as compatibility data: tests must establish that removing a hint cannot bypass the domain rule and that exported schemas remain unchanged.

## Migration Plan

Run baseline CPU/automation tests, add the policy, route both inspectors and automation through it, and validate Debug/Release plus the SceneEditing regression set in docs/Verification.md. Leave the change active and uncommitted. No persistent data migration is required.

## Open Questions

None.
