## Context

The filesystem already supports exclusive replacement and the Editor supports an unmounted Game root. Startup still unconditionally loads a JSON mount file before restoring preferences. The root preparation service currently lives in ApplicationServices, while document retirement lives in Editor and automation owns separate documents.

## Goals / Non-Goals

**Goals:** No mount configuration files; usable services with no Game root; one reusable CPU root service; explicit engine resource location; identical transition validation for GUI and agents; safe old-document retirement; portable existing asset references.

**Non-Goals:** Live Editor attachment, network transport, automatic cross-process root persistence, arbitrary remote mount editing, packaging an installed client, or scene-editing automation.

## Decisions

1. Extract root preparation/commit into Runtime/Content, depending on CPU assets/IO only. Candidate indexing reads native headers without registering concrete asset types; live type registration remains owned by the existing asset provider. ApplicationServices owns its lifetime. AssetTool can use it without linking application/graphics plugins. Preserve the typed filesystem mount layer, but delete the JSON loader and config files.
2. Applications supply an Engine content directory, defaulting to the source Content directory in development builds, with an explicit --engine-content override for tests and deployment. Engine is read-only except the existing AssetTool authoring mode. Game starts absent; --asset-root is optional and uses the same transition service. --read-only expresses a read-only Game root. Relative CLI paths resolve against the process working directory.
3. The root service validates and builds a detached candidate before modifying active state. A Main-owned participant interface reports dirty/busy state and releases old references before commit, then observes the new root. All participants pass preflight before any participant releases state. Busy work must finish before transition; dirty documents require explicit discard or saving first. Same canonical root and permissions are a no-op. A root generation rejects stale candidates and automated requests. Clear uses the same transaction with no Game mount. Scoped unregister precedes participant destruction.
4. Editor keeps dialogs and preferences; the shared service enforces transition policy. The Editor participant closes documents/history, cancels browser work and retires GPU resources; the automation participant invalidates its document map after all accepted jobs finish. Future consumers register participants rather than inserting branches into the service. Runtime/Application gains no domain behavior.
5. Expose content.root.get/set/clear through typed reflected operations, including generation, permissions, dirty/busy failures and examples. Register them at startup even with an unmounted Game root. Missing/failed/disabled assets remain a controlled unavailable capability. Synchronous root preparation retains current bounded request semantics; large index scanning can later gain a domain async contract without transport changes.
6. Editor recovers an invalid saved preference to an empty root with an error. Explicit root arguments fail on invalid directories. Viewer config does not implicitly choose a Game directory; startup clients select a root before starting scene requests. MCP/JSONL retain state within one process; single-shot commands must supply their environment per invocation.
7. Keep direct local file tool workflows and read-only/path/case/alias checks. Migrate generated mount fixtures to explicit directories or direct in-memory mounts; retain Engine substitution for missing-resource tests. Remove --mounts rather than silently accepting obsolete flags.

## Risks / Trade-offs

- Root replacement can redirect old paths to unrelated files -> participants retire documents/jobs before replacement; generation and busy checks reject stale requests.
- Preparation may reject a directory -> no active state changes. Failures during consumer retirement or reinitialization cannot restore already closed documents; hosts treat unexpected lifecycle failures as fatal rather than continuing unsafe state.
- Default no-Game changes sample commands -> migrate examples and fixtures to explicit roots; no sibling-directory fallback.
- Participant methods execute on Main -> registration/removal and transitions reject reentrancy; no work captures destroyed providers.

## Migration Plan

Add the shared service and contracts, switch startup and GUI/agent consumers, migrate tools and tests, remove old files/flags, then update documentation and run Debug/Release validation. Existing Preferences.ini values remain readable. Keep all work uncommitted and unarchived.

## Open Questions

None blocking implementation.
