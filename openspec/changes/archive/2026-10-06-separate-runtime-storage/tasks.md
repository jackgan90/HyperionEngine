## 1. Storage and configuration foundations

- [x] 1.1 Implement typed application/profile paths, native user/executable lookup and early CLI/environment overrides.
- [x] 1.2 Implement reflected revisioned storage settings, bootstrap persistence, path validation and restart-only effective state.
- [x] 1.3 Implement non-destructive configuration/state relocation and one-time known legacy import, with focused failure/conflict tests.

## 2. Reusable derived-data cache

- [x] 2.1 Add local namespaced integrity-checked records, atomic publication and recoverable failures with bounded maintenance.
- [x] 2.2 Integrate shader semantic keys and payloads without changing reflection/compilation contracts; test cold/warm/corrupt/unwritable/concurrent stores.

## 3. Application, Editor and automation

- [x] 3.1 Resolve storage before logging and route Editor configuration/state/logs/captures and shader cache through the frozen paths.
- [x] 3.2 Add shared storage provider and reflected application.storage.get/set adapter with optionality and scoped cleanup.
- [x] 3.3 Add user-data/cache editing in Editor preference with effective/saved paths, errors, override provenance and restart state; validate equivalence.
- [x] 3.4 Resolve installed Content/defaults independently of source checkout and validate explicit deployment/development selection.

## 4. Build, tool cache and test output routing

- [x] 4.1 Centralize Python tool/output paths and migrate Bootstrap/CMake/VisualStudio/profiling/content-source caches to independent locked tool storage.
- [x] 4.2 Support alternate build/output roots in build helpers and test fixture/output generation; retain existing target names.
- [x] 4.3 Isolate acceptance user data/cache/log/discovery and child processes under per-run test roots; remove runtime test pollution of repository defaults.

## 5. Validation and documentation

- [x] 5.1 Update current project documentation and usage examples without importing historical out evidence.
- [x] 5.2 Run style, semantic naming and boundary checks plus Debug/Release builds and affected unit/integration tests.
- [x] 5.3 Validate alternate output, deployment relocation, GUI/automation contracts, cache recovery/concurrency and plugin absence/shutdown paths.
- [x] 5.4 Record development verification and final worktree scope in this active change; leave archive and Git commit undone.
