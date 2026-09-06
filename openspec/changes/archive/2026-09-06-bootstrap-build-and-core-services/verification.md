# Verification

- Debug and Release: MSVC 19.50, bundled CMake 4.1.2, Ninja, C++20; builds succeeded.
- CTest: 2/2 passed in both configurations (core and dependency_boundaries).
- Core integration: real log file output, 4-thread aligned allocation stress, PMR allocation accounting, invalid alignment rejection, timing-scope accounting, zero remaining tracked bytes.
- All 15 selected upstream packages are downloaded and locked by commit/release plus SHA256. Only the core dependencies are compiled in this change.
- Build logs: `out/change01-debug.log`, `out/change01-release.log` (generated, ignored).
- Specs sync assessment: adds the new core-foundation capability; no existing capability is modified.
