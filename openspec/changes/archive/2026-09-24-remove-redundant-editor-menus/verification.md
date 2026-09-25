# Verification

- Removed the two menu drawing methods, their declarations and Edit call sites. The three retired labels and method names have no remaining references in Editor source.
- Retained Details hierarchy, shared SceneEditing transactions and Automation operation IDs. No new Outliner or viewport entry point was introduced.
- Updated Editor documentation and Automation coverage to identify deferred GUI integration.
- Debug Editor build passed: `out/EditorMenuCleanupBuild.log`.
- Existing regressions passed **4/4** in 137.17 seconds: `automation_scene`, `editor_acceptance`, `editor_multiselect`, `editor_placement`. Evidence: `out/EditorMenuCleanupTests.log`.
- Style/path checks passed across 870 owned source files; `git diff --check` passed.
- Strict change validation passed. The single consolidation requirement is synchronized during archive.
- This follow-up edits only three Editor source files, two current documentation files and its OpenSpec records/specification. Earlier uncommitted migration work and the unrelated active change remain in place. HEAD remains `1942ee6dc4456cf3a34bba1687c3a4db54e5bdd6`; no Git commit or push was performed.
