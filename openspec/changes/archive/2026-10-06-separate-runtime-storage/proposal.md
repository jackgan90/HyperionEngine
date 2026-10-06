## Why

Editor preferences, layouts, logs and shader caches currently use source-tree `out` paths independently of the selected build directory. Cleaning or relocating builds can lose user choices, and installed applications inherit development-machine paths. Storage must have explicit lifetimes and portable, overridable roots before more application and editor caches are added.

## What Changes

- Introduce startup-resolved user storage with stable application/profile identity, separate user-data and cache roots, and an independently selectable bootstrap settings file. CLI/environment overrides can avoid the system drive entirely.
- Add shared, reflected storage settings operations consumed by Editor preferences and automation. Changes persist transactionally for restart; active paths stay frozen.
- Move Editor configuration, layout, session logs and interactive captures out of default `out`; preserve existing explicit output/file flags. Migrate only known legacy preferences/layout/settings without deleting old data.
- Add a reusable local derived-data cache with validated records, atomic publication, bounded maintenance and recoverable storage failures; retain shader-owned semantic cache identity.
- Separate dependency/source download caches from build outputs; provide build/test output-root controls and isolated per-run runtime storage for acceptance.
- Support deployed Content/default configuration lookup without requiring the source checkout, with explicit development overrides.
- Leave historical `out` evidence untracked and untouched. Document only durable project-specific storage contracts; no bulk archival or generic methodology import.

## Capabilities

### New Capabilities
- `application-storage`: Root resolution, bootstrap persistence, app/profile scoping, restart-only relocation and migration.
- `derived-data-cache`: Local cache record integrity, concurrency, failure tolerance and bounded cleanup.
- `development-output-layout`: Independent build/tool-cache/test roots and reproducible isolated output.

### Modified Capabilities
- `editor-preferences`: GUI storage settings backed by the same service as typed automation.
- `shader-pipeline`: Compiler consumption of independent, recoverable derived-data storage.

## Impact

Runtime IO/storage/config/shader services, Editor bootstrap and preference UI, typed automation plugin, build/dependency/profiling/content preparation tools, CMake test registration, runtime and acceptance tests, and current documentation. Existing content mounts, serialized preference fields, operation IDs and executable/target names remain compatible. New roots are additive overrides; defaults intentionally move outside `out`. No OpenSpec archive, Git commit or push is part of this change.
