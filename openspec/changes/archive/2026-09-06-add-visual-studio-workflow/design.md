## Context
The repo uses CMake, standalone CTest executables and pinned dependency sources. VS 2022 IDE and VS 2026 Build Tools exist locally. Prior generation succeeded after normalizing inherited PATH/Path keys for MSBuild.
## Goals / Non-Goals
Goals: repeatable generation from any working directory, useful source browsing, F5 Viewer startup and complete tests from the solution. Non-goals: introducing a new test framework, Test Explorer adapters, changing the default Ninja presets or changing system settings.
## Decisions
- A PowerShell entry point delegates discovery/process execution to a Python standard-library helper. Python already supports dependency bootstrap and reliably creates a child environment with case-normalized keys. A root CMD launcher supports double-click generation.
- Auto selection prefers an installed IDE with C++ tools, falling back to supported Build Tools. Explicit 2022/2026 selection is available. A compatible bundled CMake is preferred over PATH tools; generator availability is checked.
- Each VS version uses its own `out/build/vs<year>` multi-configuration directory. Debug and Release share the solution. Normal regeneration preserves local IDE files; an explicit Fresh option refreshes CMake cache only.
- Missing locked dependencies are bootstrapped. Build/Test are optional; Test builds the dependency-aware `hyperion_check` target, which invokes CTest for the selected configuration. Child failures propagate as nonzero exit codes.
- IDE metadata lives in a separate CMake module. Owned headers, HLSL and docs appear in project filters; HLSL files are browse-only because the engine uses DXC. Per-test debugger working directories match CTest.
## Risks / Trade-offs
- No Test Explorer adapter → document `hyperion_check` and debugging standalone test executables.
- Multiple tool installations → pin generator instance and MSBuild to the selected installation, validate CMake support first.
- Repeated generation → use an existing dedicated output directory, no recursive deletion or global environment edits.
