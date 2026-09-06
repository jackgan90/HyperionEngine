# Verification

Verified on Windows x64, VS 2022 Community / MSVC 19.38, NVIDIA RTX 5080, and LLVM 22.1.1.

- `tools/GenerateSolution.ps1 -Test -Configuration Debug`: 14/14 CTest tests passed; `out/style-vs-debug.log`.
- `tools/GenerateSolution.ps1 -Test -Configuration Release`: 14/14 passed; `out/style-vs-release.log`.
- DX12 tests include clear, triangle and GUI pixel readback, window lifecycle, configuration save/load, plugin restart and execution-domain acceptance.
- `tools/Build.ps1`: Ninja Debug build passed and regenerated the compile database; `out/style-ninja-debug.log`.
- `python tools/CheckStyle.py --naming`: all 40 C++/HLSL files and 24 owned C++ translation units passed; `out/style-naming-check.log`.
- Deliberate filename, exact include casing, formatting and semantic naming violations each returned nonzero. Temporary probes were restored/removed; `out/style-negative-checks.log`.
- Root `GenerateSolution.cmd` successfully regenerated `out/build/vs2022/Hyperion.sln` from a different working directory; `out/style-cmd.log`. The legacy PowerShell child required normal host execution because the sandbox denied writes to existing CMake cache outputs. No system settings or permissions were modified.
- All 52 renamed files exist with exact casing. Dependency lock and experiment JSON data compare equal with the pre-migration versions. C++ runtime string changes consist only of renamed shader/experiment paths; `out/style-migration-audit.log`.
- Direct vendor include boundaries pass. Semantic replacement application was restricted to owned source directories; dependency sources were not renamed or reformatted.
- Remaining MSVC warnings originate in tinyexr; no owned-source compile warnings remain.

The conventions intentionally retain uppercase-first boolean names as explicitly requested. Python helper internals and CMake retain language conventions; their owned filenames use PascalCase. External ABI names, CLI flags, serialized property/plugin IDs, CMake target IDs and OpenSpec paths remain stable.
