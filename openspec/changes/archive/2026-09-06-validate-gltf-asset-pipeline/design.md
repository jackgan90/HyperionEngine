## Context

Successful parsing and a triangle screenshot do not demonstrate full asynchronous model loading. Regressions must cover IO control, supported glTF semantics and visible rendering.

## Goals / Non-Goals

Goals: Add reproducible format and lifecycle fixtures plus automated model screenshot/readback acceptance. Cover delayed IO responsiveness, one-worker progress, duplicate loads, malformed input, cancellation and shutdown. Document supported static glTF features, limitations, module boundaries and reproducible usage. Run style, naming, Debug/Release builds and complete regression suites; retain verification evidence per change.

Non-goals: no implementation of future FBX/OBJ/COLLADA codecs, animation, compressed glTF extensions, full asset cooking, ECS or additional graphics backends in this series.

## Decisions

### 1. Decision

Use deterministic generated fixtures checked into the test workflow, including asymmetric geometry, UV checker textures, alpha cutouts, nested transforms and malformed variants. Optionally compare publicly licensed Khronos samples without making normal tests require network.

### 2. Decision

Test supported semantics independently of importer implementation: known transformed bounds/vertices, archive equality, instrumented IO thread IDs, shared request outcomes and pixel regions from GPU readback.

Material/depth pixel fixtures are constructed directly as engine models so they independently test rendering rather than reusing importer output as the expected result. glTF fixture generation separately covers the import semantics. Real Viewer process tests exercise the complete path.

### 3. Decision

Exercise low worker counts and delayed providers, replacement/cancellation, loading during window resize and shutdown, and normal GPU resource lifetime. Reuse existing bounded Viewer capture hooks and add explicit model verification.

### 4. Decision

Document six-change dependency order, runtime usage, supported features and excluded extensions. Record exact commands and outcomes. Run style/path/boundary/naming checks and Debug/Release builds/tests; visual inspection accompanies pixel assertions.

Repeated parallel MSBuild validation exposed per-executable post-build DLL copy races. A shared `hyperion_runtime_files` dependency now stages runtime DLLs once per build; executable names and output directories remain unchanged. A Viewer scope guard also drains every queued save if one save fails during shutdown.

## Risks / Trade-offs

Image tests are sensitive to GPU precision; use robust region/coverage checks rather than exact full-frame equality. Generated fixtures do not establish universal glTF conformance; keep support claims scoped and list unsupported features.

## Migration Plan

Implement after `add-static-model-rendering`. Keep existing target names, serialized keys, triangle and configuration tests working. Each change is additive until its replacement paths are verified; retain explicit compatibility wrappers where required. Validate focused tests before continuing and run full Debug/Release verification at the end.

## Open Questions

No blocking product decisions. Implementation refinements must be reflected here and validated before task completion.
