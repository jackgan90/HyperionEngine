## Why

Hyperion is empty and needs a reproducible Windows C++ foundation before its rendering modules can be implemented and tested.

## What Changes

- Establish C++20 CMake targets, presets and a workspace-local dependency download workflow with immutable commits and archive hashes.
- Add engine-owned logging, tagged aligned memory allocation, and profiling wrappers backed by spdlog, mimalloc and Tracy.
- Add executable integration tests and a check that third-party headers remain inside adapters.
- Record the nine-change delivery roadmap.

## Capabilities

### New Capabilities
- `core-foundation`: reproducible builds, isolated dependencies, logging, allocation and diagnostics.

### Modified Capabilities

None.

## Impact

Adds top-level build files, tools, public engine headers, private adapter implementations, tests and project documentation. No system-wide installation is required.
