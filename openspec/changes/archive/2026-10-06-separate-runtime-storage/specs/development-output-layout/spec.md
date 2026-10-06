## ADDED Requirements

### Requirement: Build and test output ownership
Supported build and acceptance entry points SHALL route new default repository out outputs into build and tests. Existing locked source-recipe generator entry points SHALL retain compatibility, with these entry points receiving explicit output directories from build/acceptance callers. Owned tools SHALL accept an alternate output/build root consistently. Runtime defaults SHALL NOT depend on that root. Historical out evidence SHALL remain untracked and SHALL NOT be bulk archived or added to version control by migration.

#### Scenario: Alternate clean output root
- **WHEN** tooling builds and tests using a new output root
- **THEN** necessary outputs are regenerated there while interactive preferences and runtime caches remain unchanged

### Requirement: Independent locked tool caches
Dependency downloads, pristine dependency sources and Engine source-asset downloads SHALL use an independently overridable tool cache. Game source caches remain owned by their selected external asset environment. Build products SHALL remain under the selected build root. Existing lock hashes SHALL validate reuse and legacy import; mismatches SHALL NOT be silently trusted.

#### Scenario: Offline rebuild from retained sources
- **WHEN** a build output directory is absent but verified dependencies remain available
- **THEN** build setup can reuse those sources without depending on the old out directory

### Requirement: Complete acceptance storage isolation
Acceptance runners SHALL provide isolated per-run configuration, state, cache, logs, discovery and artifacts inherited by child Editor/CLI/MCP processes. Isolation SHALL use explicit engine overrides and SHALL NOT depend solely on spoofing a platform environment variable.

#### Scenario: Parallel interactive and acceptance sessions
- **WHEN** acceptance runs while an interactive user has saved preferences and targets
- **THEN** acceptance neither consumes/modifies those preferences nor publishes discovery records into the user's environment
