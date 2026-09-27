## Context

SceneEditing owns document identity, ordered selection and history. Scene provides atomic AddNodes; Renderer supplies resolved resources. Existing DuplicateNode changes one node and shares resource bookkeeping. Platform exposes only text clipboard functions; the bundled SDL Windows backend keeps unknown MIME data process-local. GUI and automation must use one domain implementation.

## Goals / Non-Goals

**Goals:** immutable multi-selection subtree capture; exact authored values except numbered names and identities; authoritative system clipboard; atomic paste/history; shared automation; explicit lifecycle and error contracts.

**Non-Goals:** cut, duplicate command changes, cross-document/process/restart paste, asset duplication, world-space placement, automatic scene camera/light activation, archive and commit.

## Decisions

1. Document-scoped clipboard service lives with FSceneEditDocument and accepts injected typed clipboard read/write functions. Platform implements byte/string format transport without scene dependencies. Editor injects its window provider. Automation calls the same document operations. Missing providers remain discoverable but return unavailable. All calls run on Main.
2. The OS clipboard contains a versioned format and random per-copy token; owned immutable nodes stay in the document. A text name list is offered alongside it. Paste always reads the actual system format and matches the current token. Ordinary text/images/files invalidate object paste. Tokens never encode pointers or grant cross-process access. Windows uses registered native formats in the private Platform adapter because the bundled SDL fallback is insufficient. Clipboard failures never fall back to a cached token.
3. Capture ordered selection, deduplicated roots and descendants once, using iterative traversal. Freeze mutable material instances, retain immutable model/texture/sky resources and copy authored override values. Built-in components have known value/reference semantics. Registered extension components require an explicit clipboard clone/reference contract; unsupported or opaque components reject the entire copy. Bound snapshots to 16,384 nodes and 64 MiB of authored snapshot data (shared bulk resources excluded). Failed or empty copy does not replace the previous snapshot.
4. Paste preserves original external parents and Local matrices. Internal parent and ModelSource.InstanceRoot references remap through the fresh node-ID table; asset source IDs remain stable. Missing external references fail the batch. External handles follow history remapping, preventing same-ID replacement from silently becoming a parent. No inverse/decomposition is needed, including singular transforms. Parent changes after copying affect inherited world state normally.
5. Names use scene-wide collision-free numeric suffixes: Name, Name (1), Name (2). Pasting a suffixed name advances that suffix; ordinary trailing digits remain part of the base. All node names are allocated against existing and candidate names. New paste allocates new IDs; redo reuses its recorded IDs and names.
6. Extend history with created-node batches and before/after selection. Prepare allocation and validation before atomic AddNodes, then publish one history entry and selection update. Undo removes all created roots; redo restores captured nodes and simultaneously remaps every history/selection handle. Clipboard reads occur only on a new paste, never redo. Copy leaves dirty/save-point/redo unchanged. Scene settings remain unchanged.
7. Object shortcuts use focused scene panels, exact Ctrl+C/Ctrl+V, key-down without repeat, text-edit ownership and existing modal/drag/placement/gizmo guards. Text copy/paste remains with GUI, including the frame that ends text editing. Asset windows and Content Browser do not route scene shortcuts.
8. Document reset/detach and content-root retirement release snapshots, without clearing unrelated OS data. Saving the same document does not invalidate the clipboard. Live resource updates rebind authored values through the existing Renderer path before admission; incompatible/unrepresentable resources fail without partial nodes. Resource preparation uses shared busy checks. A terminal failed resource is copyable only when complete authored state remains available.
9. Add reflected scene.selection.copy, scene.clipboard.info and scene.clipboard.paste operations. Results are bounded summaries; callers query scene.selection.get / scene.nodes.list for created objects rather than returning huge payloads. Validate document/revision for mutations and expose completion as logical commit, not GPU readiness. Transport implementations remain unchanged.
10. A successful GUI placement drop transfers keyboard focus from Place Object to Viewport after the shared placement commit. This makes the newly selected object immediately usable by scene shortcuts. Keep this focus change in GUI delivery, so automation placement does not steal focus and cancelled/failed drags do not transfer it.

## Risks / Trade-offs

- Parent deleted after copy -> reject rather than silently changing hierarchy/local values; undo restoration remaps captured external handles.
- Clipboard APIs can replace some formats before failure -> report failure, never install a new local snapshot unless write succeeds, and always validate the real clipboard on subsequent paste.
- Shared assets can change after capture -> freeze object overrides, retain asset identity and rebind current resources; no new asset-version storage.
- Custom components can contain hidden mutable pointers or node references -> explicit component clone/remap opt-in, reject unsupported types.
- Document-local tokens do not survive switching scenes or restarting -> explicit unavailable state and documentation; portable payloads are a later capability.

## Migration Plan

Add the interfaces and batch history path, then platform transport, GUI/automation adapters and regression coverage. No persisted schema or existing operation changes. Rollback removes only the new capability; saved pasted nodes remain ordinary scene nodes.

## Open Questions

None. Product decisions follow the accepted investigation recommendations, including numbered names, original parent/local transform, subtree semantics and same-document scope.
