## Why

SceneBridge represents status revision components and material dependency records as positional pairs. Its material version vector also interleaves section identifiers with snapshot identities, so readers must reconstruct an undocumented sequence before assessing cache equality. This stage names those existing relationships without changing publication or invalidation behavior.

## What Changes

- Replace the two status-revision pair returns with a named Renderer value recording bridge status and resource publication revisions.
- **BREAKING (C++ source only):** callers that explicitly name the former pair type must adopt the named value; update all repository consumers together. No reflected or serialized contract changes.
- Represent model-level and section-level material snapshot revisions explicitly, with a named record for each editable material dependency.
- Preserve the existing change-mask gate, material freezing, comparison semantics, publication admission, receipt handling and resource lifetimes.
- Add focused behavior regressions and document the status/cache meanings.

## Capabilities

### New Capabilities
- `scene-bridge-state-keys`: Named bridge status and material dependency records with equivalent invalidation and publication behavior.

### Modified Capabilities
None. Existing scene, material, automation and lifecycle requirements remain unchanged.

## Impact

Renderer SceneBridge and SceneInstance's model-status cache, focused Renderer tests and Scene management guidance. No new Runtime module, plugin dependency, user operation, persistence field or shader contract. The maintainability draft remains excluded from Git commits.
