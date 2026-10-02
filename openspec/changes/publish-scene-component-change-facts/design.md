## Context

M07 was rechecked against `SceneBatch.cpp::StageNode`, `SceneMutation.cpp::PrepareChanges/Commit`, `FSceneChange`, `FSceneComponents::All`, and synchronization/clear paths in `Scene.cpp`. Registry descriptors already own stable type IDs and equality callbacks. Instances have independent IDs and some types permit multiple instances. Empty optional slots occur in the container and are not live components. Unknown archive envelopes are separately retained as opaque values.

Current change maps merge masks by generation-bearing node handle and keep the latest node snapshot. The first authoritative mutation happens after pending changes, query dirtiness and allocations are prepared. This property is a required boundary. Renderer and query consumers already use distinct compatibility effects and do not need to become generic component runtimes.

## Goals / Non-Goals

**Goals:** Make every registered live component's committed addition, modification and removal observable with owned identity; retain occurrence facts until acknowledgement; preserve efficient built-in consumers and transaction behavior.

**Non-Goals:** ECS conversion, renderer/query behavior inferred from arbitrary registration, extra synchronization consumers, ordered event history, interpreting unknown archive envelopes, new editor/automation operations, persistent or external wire schema changes, or broad performance instrumentation.

## Decisions

### Owned facts alongside the latest snapshot

Add a small component change value to `FSceneChange`, containing owned type ID, owned instance ID and Added/Modified/Removed flags. Facts are keyed by both IDs; equality is descriptor equality for matching live instances, never `Slot<T>()`, vector order, raw memory, or `std::any` identity. Repeated component types remain distinct. Rename is removal of the old identity plus addition of the new one. A type change under the same instance ID is also removal plus addition of separate type identities. Empty optional slots are ignored.

Facts describe successful authoritative commits since the consumer's acknowledgement, not transient edits to an unpublished draft. An add and remove entirely inside an unchanged draft produces no published fact. Across successful commits, add then remove retains both flags; remove then readd retains both, and modification can coexist. The latest `Node`/`bRemoved` remains the final state. The list is deterministic and has one entry per identity, but is not an ordered history and carries no deleted payload. Owned strings remain valid after node destruction. Generation-bearing node handles keep old-node removals separate from a reused slot.

Unknown opaque archive envelopes remain preserved by the current archive/container contract and continue to cause Metadata changes; they are not registered typed instances and do not gain interpreted component facts in this change.

### One difference computation, explicit compatibility projection

A focused private difference helper enumerates live registered components of touched nodes and invokes the existing descriptor equality callback. Use it for staged edits, addition and removal, including Clear and subtree paths. Initial synchronization explicitly contributes Added facts for every current live component, merged with any pending facts without advancing the scene revision.

Built-in Model/Camera/Light effects remain an explicit Scene-private compatibility projection from the same differences. They are not declared renderer policies on registry descriptors. Preserve the existing semantic built-in comparisons: an identity-only rename whose unique built-in value is unchanged must not unnecessarily rebuild model resources or change effective camera/light data. Local transform, parent relation and inherited world/enabled effects retain their existing dedicated handling; a child's derived world update is not falsely a mutation of its local component. Opaque or custom metadata changes do not gain model/query invalidation.

It is acceptable to retain focused typed comparisons for the affected built-in compatibility projection; generic fact discovery must not depend on that fixed list. This avoids conflating extensible evidence with a promise that every arbitrary component affects every consumer.

### Prepare before publishing

Compute and merge component facts in the mutation's owned scratch state, before the first authoritative write in Commit. Descriptor equality or allocation failure leaves node values, relationships, settings, pending changes, revision, query index and history unchanged. Keep the current single-consumer acknowledgement semantics: an entry with a newer merged revision survives acknowledgement of an older revision, retaining its pending facts as current masks already do. No user callback may run after the publication boundary.

No per-frame scan is added. Work is proportional to registered components on nodes already touched by a mutation, plus those enumerated for initial synchronization. Existing bridge masks and query dirty effects stay incremental; unchanged/custom-only nodes do not trigger global rebuilds.

### Verification through domain consumers

Add fixed existing mask/revision/metadata baseline assertions before production edits. New tests register a custom repeated type and inspect actual `GetChanges()` output for add/modify/remove/rename/type replacement, no-op and empty-slot cases. Cover unacknowledged merges, old acknowledgements, removed nodes, Clear, subtree deletion, initial synchronization, slot reuse and throwing descriptor equality. Retain an old returned changes vector across later edits to prove owned snapshots/identities.

Exercise SceneEditing Execute/Undo/Redo through existing shared domain operations, and verify the emitted facts rather than reproducing diff logic in tests. Existing scene bridge metadata reuse and spatial-query rebuild/refit counters must remain unchanged for irrelevant metadata; meaningful transform/model edits retain current updates. Run the relevant `scene_management`, `scene_editing`, `scene_rendering`, `scene_ray_queries`, `scene_dispatch_failure` and `scene_boundary_contracts` targets after resolving their actual CMake names. No rendering implementation change is required unless a verified consumer gap appears.

## Risks / Trade-offs

- Facts can grow while acknowledgement is delayed → merge by identity, document occurrence-set semantics, and keep current single-consumer contract.
- Built-in identity changes can cause accidental extra invalidation → independent mask/reuse assertions separate value effects from identity facts.
- New allocations could weaken transaction guarantees → prepare facts and callback results before publication; inject equality failure and verify unchanged state.
- Parallel M11A affects reflection member association → coordinate build snapshots and shared tests; M07 does not change reflection registration or schema.

## Migration Plan

Freeze existing masks/history/query behavior with tests, implement facts and merge preparation, then migrate only the private compatibility projection as needed. Update SceneComponents docs and verify tests in Debug/Release, style/naming/boundaries and independent review. Reverting the change restores the old C++ stream without asset migration. No new public operation requires an automation adapter.

## Open Questions

None for scope. Concrete helper/type names and efficient deterministic collection representation are implementation decisions constrained by the observable contract above.
