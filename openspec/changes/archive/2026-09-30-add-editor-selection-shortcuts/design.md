## Context

FSceneEditDocument owns the ordered FSceneSelection consumed by Editor and Automation. GUI clicks currently call ReplaceSelection; scene.selection.set validates the entire list but has an unrelated 128-object limit. Outliner rows share FReparentGesture press/release arbitration, and tree traversal differs from flat search results. Gui already receives Shift but does not expose it in FGuiPointerState.

## Goals / Non-Goals

**Goals:** Implement scene-wide Ctrl+A, displayed-order Outliner ranges and viewport Shift toggle; preserve primary, transactions, input ownership and GUI/agent equivalence.

**Non-Goals:** Box selection, model-internal primitive selection, new renderer queries, arbitrary axis pivots, plugin or application lifecycle changes, archive or Git commit.

## Decisions

1. Ctrl+A selects every live logical node from ISceneEditTarget::Nodes, independently of filtering, expansion or rendering eligibility. Preserve a live current primary by placing it last; otherwise use the stable node enumeration's last element. Empty scenes produce empty selection. This gives identical results in both panels.
2. SceneEditing exposes validated batch selection and SelectAllSceneNodes. Remove only the explicit-selection 128-object restriction; duplicate, stale/foreign-handle, document, revision and idle checks remain atomic. Other editing limits and transport budgets remain unchanged. Register scene.selection.select_all with the existing reflected FSceneMutationRequest and a new reflected count/primary summary so large server-side selections have bounded responses. Explicit ranges use scene.selection.set; UI row order is presentation state, not a simulated-input RPC.
3. A small Editor-owned Outliner selection state tracks a generation-safe anchor. Each frame records all submitted object rows, including scrolled-offscreen rows, excluding collapsed descendants and search exclusions. Shift replaces with an inclusive interval; Ctrl+Shift unions it with current selection. The endpoint is last/primary even for reverse ranges. Non-Shift object clicks establish the anchor; repeated Shift clicks preserve it. Missing anchors fall back to a displayed current primary, then the clicked endpoint. Filter changes, document replacement/history and external selection reset the anchor; handles are revalidated against current rows.
4. Range application occurs after Outliner traversal using the existing release-versus-drag arbitration. Shift presses must not prematurely clear selection. Dragging a selected row retains its original group; a gesture that becomes a drag must not also perform a range click. Keep keyboard activation, expansion arrows, cancellation and scoped reparent payload behavior. Domain submission occurs after gesture cancellation and interaction-state refresh.
5. Expose bShift through the Gui wrapper and capture Ctrl/Shift at mouse press. Viewport uses Ctrl-or-Shift for the existing toggle policy, including lights and miss/unavailable results. Outliner keeps the two modifiers distinct. Ctrl+A runs once per non-repeat key press only when Viewport or Outliner has focus, no text owner/popup/foreign window exists, and no conflicting navigation, placement, gizmo or hierarchy interaction owns input. Use existing frame-owned text guards.
6. Keep all scene commits on Main. Runtime/SceneEditing remains CPU-only; Runtime/Application, renderer ownership, plugin dependencies and registration lifetimes remain unchanged. Automation registers through the existing automation-scene owner before session sealing and uses its existing scoped cleanup/unavailable behavior. Follow current CodingStyle boolean rules over the stale boolean sentence in openspec/config.yaml.

## Risks / Trade-offs

- Incorrect range order or incomplete traversal → collect the actual current frame's complete submitted rows, apply after drawing, test nested/filter/reverse/offscreen cases.
- Selection versus drag interference → retain the established press/release ownership model and test selected-group drag, cancelled Shift drag and modifier release.
- Stale anchor or slot reuse → store full handles, reset on document/history/external selection, resolve endpoints against current rows and validate atomically.
- Large selection response budgets → select-all returns only document/revision/count/primary; existing explicit get/set remain subject to documented transport budgets. Do not broaden unrelated batch-edit contracts.
- Shortcut stealing text or navigation → use frame-owned text, panel focus, popup and existing busy/gesture guards and run real Editor regressions.

## Migration Plan

No persisted data migration is needed. Existing IDs and field semantics remain stable; accepting larger explicit selections is a compatible capability expansion. Implement shared services, GUI routing, discovery/invocation and real-input regression coverage, then update documentation and validate Debug/Release. Leave the active change and source edits for user review.

## Open Questions

None blocking implementation; the user approved the proposed default semantics.
