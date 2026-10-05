## Context

The existing checker reads unconfigured CMake tokens, ignores link visibility and skips test dependency checks. Renderer public NativeModel and RenderOutput interfaces include Assets types despite a private link declaration, and GuiRenderer's public Diagnostics interface includes Core types despite a private link declaration. These contracts require public dependency propagation and consumers that link only the owning target. Windows x64/MSVC and existing targets remain stable.

## Goals / Non-Goals

**Goals:** use actual expanded configure commands to identify targets, sources and direct link visibility; keep production and test ownership separate; reject stale/unsupported input; fix verified declarations; provide fixtures, public consumers and current ownership documentation.

**Non-Goals:** engine features, broad module movement, new renderer layering or prohibiting the current intentional RHI→Assets edge.

## Decisions

- Capture CMake's JSON expanded trace during supported configure entry points and export a compact graph. CMake evaluates variables, loops and conditions, avoiding an independent interpreter. Sources and edges come from executed `add_library`, `add_executable`, `target_sources` and `target_link_libraries`, with declaration locations retained. An unconfigured token parser and transitive file-API link graph were considered but cannot establish direct visibility reliably.
- Support direct module CMakeLists declarations and the existing hyp_test helper after checking its trace caller belongs to Source/Tests. Fail explicitly for source/link generator expressions, property-based graph mutation (including direct injection), directory-level link inheritance, PUBLIC/INTERFACE source propagation, unknown helper target declarations or ambiguous relative sources. Dependencies.cmake's third-party wrapper/imported declarations are outside the owned graph. Generator expressions in compile options, tests and unrelated third-party graphs remain CMake's responsibility.
- Mark executables outside application directories and targets in `Source/Tests` as test/verification targets; exported metadata records the reason. Shared implementation sources retain their physical module/private boundary; production checks run for every production compilation owner. Tests retain private and SDK checks without imposing production layering on test-only links.
- Bind metadata to configured root, configuration/options, CMake input hashes and owned source inventory. Checking requires fresh successful configuration. One invocation proves only its named configuration; Debug/Release, test selection and optional plugin branches are validated separately.
- Public engine includes require a direct PUBLIC/INTERFACE edge; private production includes require a direct link edge. Layer constraints and cycle detection use actual production edges. Disabled modules are explicitly reported as outside the configured graph while SDK/private source isolation is still scanned.
- Only a known boolean build-option guard can establish an inactive include for direct-dependency checks; unknown expressions inspect both branches conservatively. Comment/string content is not a preprocessing directive. Private/vendor isolation scans every real include. Uncompiled CPP files can reference existing Source/Tests support headers as source-only coverage; any production compilation owner still rejects this dependency, and no filename-based test identity is inferred.

## Risks / Trade-offs

- [A new CMake graph syntax is unsupported] → fail with its command/location and extend the collector with a fixture before adoption.
- [Configuration-specific branches are missed] → print active options and omitted modules; validation runs the affected configurations and BUILD_TESTING selections.
- [Existing declarations exposed by stricter checks] → verify each include/owner before minimal declaration fixes; do not add broad exceptions.
- [Test source shared with production] → retain per-target source assignments and apply production rules whenever any production owner compiles it.
