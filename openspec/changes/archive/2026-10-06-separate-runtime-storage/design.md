## Context

Editor constructs runtime paths from HYP_SOURCE_DIR before and during plugin composition. IO already supplies atomic local writes and leases; Config owns reflected application settings; shaders already have semantic SHA-256 keys. Content indexes all mounted roots, so adding a user mount would conflate asset identity with mutable local state. Runtime/Application remains a minimal lifecycle host.

## Goals / Non-Goals

Goals: independent user data/cache roots, entirely off-system-drive startup, shared GUI/automation settings, safe restart-based relocation, reusable local DDC, isolated tests, independent development download caches, and deployable Content lookup. Existing fields, flags, mounts and plugin disablement contracts remain compatible.

Non-goals: remote cache servers, live replacement of open storage roots, project-format invention, general-purpose settings editor, packaged shader cooking, historical output archival, OpenSpec archive, Git commit or push. Historical evidence stays untracked; only current project-specific contracts enter documentation.

## Decisions

### Paths and bootstrap

IO owns lightweight application-path values and private Windows Known Folder/executable-location adapters, with no SDL dependency. Config owns the reflected storage settings service and persistence. Apps select a stable AppId and Profile (default Editor/Default). Native configuration directory lookup uses LocalAppData; explicit --storage-settings, --user-data-root, --cache-root and matching HYP_* environment overrides are resolved before logs. Selecting an explicit bootstrap file permits all startup writes outside C:. Explicit roots take precedence over persisted choices; saved and effective values plus override provenance are visible.

UserDataRoot contains Apps/<AppId>/<Profile>/{Config,State,Logs,Captures,Data}; CacheRoot contains DerivedData and application-specific caches. Bootstrap settings are a small stable locator with schema/revision and roots, not a content mount configuration. CLI/file overrides remain highest priority. Roots are frozen for a process lifetime. Editor can save future roots through one service; candidate paths are normalized, checked for dangerous overlap and probed for writability before atomic persistence. Stale updates are rejected, including cross-process changes under a write lease. Empty saved root resets to the platform default. No live root replacement or content-generation changes occur.

Default locator is outside the redirected data tree. With explicit user root and no explicit locator, a locator under that root avoids consulting the system folder. --storage-settings is the recommended stable anchor for GUI-managed relocation. CLI/environment pinned roots remain authoritative and are disclosed; saving does not silently rewrite launch arguments.

### Migration and configuration

Known old Editor preference/render/UI/layout files are imported only into absent new targets, using validated existing loaders where applicable. An explicit isolated/test environment suppresses legacy import. Old files are retained. User-root relocation records the previous application directory and copies Config/State at the next startup, after the previous orderly shutdown has saved layouts; destination files win and linked entries are rejected. A one-time completion marker prevents repeated resurrection of removed preferences. Logs, captures, caches and arbitrary old out files are never bulk migrated. Original and destination roots are never recursively deleted.

Existing serializers and domain validators remain authoritative. Common storage bootstrap uses reflection and atomic IO; no duplicated shader or rendering fields. Normal default settings are code defaults, optionally supplied by shipped files when user files are absent, then overridden by existing CLI/session options. A fresh install resolves Content from the executable/install layout before an explicit development source fallback. Deployed builds can disable the fallback and can run from a read-only installation with user storage elsewhere. Required shipped artifacts are not optional DDC.

### Local DDC

Runtime/DerivedDataCache supplies a local record store over IO, including a reusable digest function with native hashing private to its adapter. Shaders construct semantic keys and retain compilation/reflection behavior; the cache has no Shader/Renderer dependency. Namespaces and validated hex keys confine records beneath the cache root. Records contain a format marker, payload length/integrity hash and immutable bytes. Unique temporary files and atomic replacement prevent partial publication; competing identical producers are safe. Read/write failures are misses/best-effort publication with bounded diagnostics, while actual compilation failures propagate. Explicit maintenance removes only recognized records under its namespace by age/size, with a bounded per-call scan. It cannot delete configuration or authored content. Initial policy is local-only; old shader cache files can be discarded rather than trusted as a new storage format.

### Plugins, Editor and automation

Editor constructs storage before logs and passes the owned service into composition. A scoped storage provider publishes the typed service without coupling the minimal host to features. The preference dialog shows active/saved roots, locator, override state and restart requirement, and submits a reflected revisioned edit through the shared service. A dedicated optional automation adapter registers application.storage.get/set before automation-session seals the catalog and unregisters on rollback/stop. Missing/disabled providers produce controlled unavailable responses. No transport-specific branches. Tests cover discovery, invocation, stale/invalid/failed saves and equivalent GUI domain calls.

### Development outputs

ToolCache is separate from runtime DDC. One Python path contract resolves HYP_TOOL_CACHE and HYP_OUT_ROOT (with command overrides) for bootstrap/build/profiling/content helpers and CMake. Locked downloads and versioned pristine dependency sources live in ToolCache; writable build products live in out/build. Existing out/deps is an explicit migration input, not a permanent fallback. Import checksum-verified existing archives when available to avoid needless downloads; no deletion of historical dependencies. Build directory overrides remain compatible with direct cmake -B and configured HYP_DEPS.

Test runners supply one run root under out/tests with user data, cache, logs, discovery and artifacts inherited by children. Discovery retains same-user ACL and target protocol. Editor and CLI share a stable locator-relative discovery base, so pending root changes do not hide a still-running target; the explicit HYP_DISCOVERY_ROOT override remains available. Test isolation freezes ToolCache before changing compatibility LOCALAPPDATA. Test fixture and supported build/acceptance tool outputs use the selected output root, not compiled repository out paths. The two source-recipe generators retain their byte-for-byte locked implementation and legacy standalone defaults for existing HyperionAssets manifests; callers explicitly provide their output directories. Legacy relative test caches run beneath the build/test directory, never the repository root. Long-term project documentation explains this policy without importing generic methods or old task transcripts.

## Risks / Trade-offs

- Restart-only changes delay relocation until next launch -> show active versus saved/effective-next state and require a stable bootstrap anchor.
- Multiple Editors can update the same settings -> lease plus persistent revision comparison; configuration format migration is explicit.
- A read-only system drive prevents default locator writes -> explicit locator/user/cache overrides are processed without accessing that default.
- Destination preferences can already exist -> copy missing files only, keep originals, report failures and never overwrite unrelated data.
- Shared caches expose interrupted writers and cleanup races -> atomic publication, bounded reads, immutable keyed payloads, cache failures remain recoverable.
- Download defaults change -> explicit ToolCache override and lock-verified legacy import; no claim of offline regeneration without retained sources.
- Native GUI/GPU acceptance depends on desktop/device availability -> run headless contracts independently and report any unavailable live checks precisely.

## Migration Plan

Implement and test path/config foundations first, then cache, then app/plugin/UI integration, then tool/test routing. Keep single-file overrides and old serializers. Validate a clean alternate output root, copied deployment with source fallback disabled, warm/cold/cache-failure runs, and unchanged old evidence. Document exact developer commands and preserve active OpenSpec artifacts for user acceptance.

## Open Questions

None blocking implementation. Persistent project identity and cooked shader distribution are deliberately separate future work.
