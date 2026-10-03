# Verification

## Scope and baseline

- Base HEAD: `0f165ce694d28284ed4e1ea43ff29142d32f07dd`; the preceding `separate-material-variant-execution` change remains uncommitted and is outside this change.
- Added `ImportValidationTests.cpp` to the existing `asset_import_workspace` test. After correcting asynchronous result pointer access in the new test, built and ran it against the unchanged production implementation: Debug **1/1 passed**, 3.93 seconds. Logs: `out/maintainability/ImportRulesBaselineBuild.log` and `ImportRulesBaselineTests.log`.
- No archive, commit, staging or push is part of this change.

## Public-entry coverage

- Source identity: root only, ID only, simultaneous incomplete/invalid ID (pair error wins), colon, backslash, leading slash, traversal and embedded double-dot; omitted identity, slash-separated names, ordinary dots and UTF-8 names accepted.
- Exact Workspace invalid_arguments code/message; exact service invalid_argument message; service identity errors remain asynchronous after synchronous admission. Rejected cases perform zero writes and create no output.
- Bad output extension, normalized physical source versus mounted output equality, Engine output/library rejection, Workspace sourceId length/NUL bounds.
- Uppercase .HASSET, normalized output/default library, explicitly equivalent library, separate library, read-only PrepareAsync, unchanged reimport ID and zero writes.
- Grouped Workspace explicit-library rejection, independent service separate-library rejection and equivalent-library acceptance; predicted folder matches actual publication.

## Manual review

- `IFileSystem::Normalize`, `ImportPath`, `ImportExtension` and `IsAssetIdentifier` retain their existing authority. ImportRules owns only the duplicated decisions and default-library derivation.
- Source identity outcome uses a named enum; diagnostics remain at their existing caller boundaries. Boolean root presence expresses an input fact, not a switch between endpoint policies.
- No input format, accepted-name policy, public ABI, reflected operation, schema, revision, cache key, notification, thread routing or plugin lifecycle is changed. Workspace content-root admission and service capacity/closing checks stay in place.
- Source-root normalization and check order are unchanged. Publish still acquires its existing leases and keeps transactional Build/Commit and freshness behavior. PrepareAsync only replaces library normalization and gains no publication checks.
- All changed production C++ files remain below 500 lines; changed/new functions are below 100 lines. The existing Workspace test file is test code and gains only a declaration and call.

## Final verification

- Built `import_workspace_tests`, `publication_tests`, `shared_asset_publication_tests`, `import_tests`, `model_import_tests`, `hyperion_asset_tool`, `hyperion_automation_cli` and `hyperion_editor` in Ninja Debug and Release with MSVC 14.50.35717. BUILD_TESTING=ON, Tracy=OFF; RenderDoc ON in Debug and OFF in Release. Both selected build logs contain no compiler warnings or errors.
- Regression selection: `^(asset_import_workspace|native_asset_publication|shared_asset_publication|native_publication_cli|editor_asset_import|import_draft_automation|async_gltf_import|model_material_conversion)$`; CTest includes `model_fixtures`, totaling nine tests.
- Debug: **9/9 passed**, 95.85 seconds. Release: **9/9 passed**, 31.60 seconds.
- Existing Editor import acceptance exercises JSONL/MCP discovery, descriptions and calls as well as real Editor widgets. Existing draft acceptance exercises preparation, inspection, property edits/history, submission and discard. Existing native publication suites retain transaction, shared-identity, dependency and zero-write reimport coverage.
- Full `CheckStyle.py`: paths and formatting passed (1112 sources). `CheckBoundaries.py`: passed (1073 sources, 39 modules). Semantic naming/declaration checks passed for all six translation units changed by this task using the Debug compilation database and VS environment.
- Manual physical line counts: ImportRules.h 23, ImportRules.cpp 36, ImportValidation.cpp 154, AssetPublication.cpp 150, ImportPreparation.cpp 83.
- `openspec validate --all --strict`: **110/110 passed**. `git diff --check` passed.
- Raw logs: `out/maintainability/ImportRules{Debug,Release}{Build,Tests}.log`, `ImportRulesStyle.log`, `ImportRulesNaming.log`, `ImportRulesOpenSpec.log`.
- Final state: seven tasks complete, including the audit follow-up below; both this change and the preceding material change remain active, HEAD is unchanged and the Git index is empty. No archive, commit or push was performed.
- Limits: this was the affected regression set, not a full `hyperion_check` or Visual Studio solution regeneration. No performance claim is made for this structural change.

## Independent audit follow-up (2026-10-03)

- A reviewer with no inherited session context checked this change against the baseline and its direct call chains. No confirmed production defect or semantic regression was found. Initial scope hashes matched `out/maintainability/AuditMaterialImport/InitialSnapshot.json`; the main agent separately verified helper equivalence, error ordering and preparation/publication boundaries.
- IR-A01 was an optional P3 test-coverage suggestion, not a production defect. The main agent confirmed that the existing PrepareAsync assertions did not directly protect its different admission rules, then added `CheckPreparationAdmission` in `ImportValidationTests.cpp` without changing production code.
- The new public-entry regression proves that read-only preparation succeeds for a non-.hasset output while ImportAsync rejects synchronously, and that root-only source identity prepares successfully while publication rejects through its asynchronous result. Both paths assert zero writes and absent output files.
- Rebuilt `import_workspace_tests` in Debug and Release. Debug `asset_import_workspace`: **1/1 passed**, 4.22 seconds; Release: **1/1 passed**, 1.64 seconds. Changed-file formatting and semantic naming passed, and both selected build logs contain no compiler warnings/errors. Logs: `out/maintainability/AuditMaterialImport/ImportAdmission{Debug,Release}{Build,Tests}.log` and `ImportAdmissionStyle.log`.
- The original reviewer checked the new test, exception boundaries, actual entry invocation and logs against `AfterTestSnapshot.json`, and passed targeted re-review with no new defect. Only the test file differed from the initial snapshot before recording audit results.
- The optional suggestion's additional direct observation of library paths in a dependency-producing preparation was not added. Existing publication library tests and static normalization review remain the evidence for that aspect; this is not an unresolved confirmed defect.
