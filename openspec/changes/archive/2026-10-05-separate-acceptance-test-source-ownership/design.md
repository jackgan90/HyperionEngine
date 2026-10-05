## Context

The Editor plugin builds acceptance implementations only with BUILD_TESTING, but their source files and unit test entry points reside alongside production Private code. FEditorAcceptanceDriver already separates production observation calls from a forward-declared harness. A test-disabled implementation rejects requested exercise modes in a controlled way. The boundary script currently infers ownership from filename patterns.

## Goals / Non-Goals

Make ownership visible in the directory structure and build inputs while preserving all exercise flags, reports, timing policies and production behavior. Do not redesign scenarios, remove observations or introduce a new plugin/lifecycle.

## Decisions

### Editor-owned test directories

Move harness, state and all test-enabled driver/scenario implementation into Tests/Acceptance. Move unit test entry points and their dedicated support header into Tests/Unit. Keep EditorAcceptanceDriver.h, observation types, report implementation and EditorAcceptanceUnavailable.cpp in Private because they form the production boundary.

Test sources use explicit relative includes to production private headers. Production gets no Tests include directory. Targets and dependencies stay unchanged; CMake explicitly lists acceptance sources inside BUILD_TESTING. The unavailable implementation stays selected only when testing is disabled.

The general source-boundary checker permits sources under a module's Tests directory to include that same module's Tests headers. Production Private/Public sources and tests from other modules cannot use that exception; production Private headers remain available to the owning module's tests.

Assign explicit configured headers and the transitive local-header closure to their actual consuming targets. A test-only helper does not inherit the module's production dependencies; a header consumed by multiple production targets must satisfy each consuming target's direct links. Public headers retain the exporting module's PUBLIC/INTERFACE requirements, while unselected non-test headers retain module-contract coverage. Test targets remain outside the production dependency graph. Directory ownership and native/vendor isolation are checked for every header, including unselected module Tests headers; dependency attribution does not grant a private-header exception.

### Verify logical ownership and actual compilation

Guard production local-include closure against reaching Tests or scenario headers. Inspect the configured Editor source list and compile_commands.json using source paths instead of scenario filename heuristics. Require every acceptance implementation when enabled and no Tests source in the production plugin when disabled. Cover rejection with small negative source-selection/include fixtures and retain existing production scenario-branch checks.

## Risks / Trade-offs

- Relative includes can break after movement → update local references from observed include paths and compile all test targets and applications.
- Filename rules can silently miss new scenarios → derive expected sources from the dedicated acceptance directory.
- OFF builds may accidentally reference harness symbols → build and exercise the actual OFF application with both normal startup and controlled unsupported exercise requests.

## Migration Plan

Move sources without behavior edits, update CMake/includes and boundary tooling, validate ON/OFF builds and existing acceptance regressions, then independently audit the resulting scope.
