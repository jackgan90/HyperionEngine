## Why

The repository currently mixes lower-camel identifiers, snake-case filenames, and LLVM formatting. The owner wants Unreal Engine conventions as the default while the renderer is still small enough to migrate consistently.

## What Changes

- **BREAKING**: Rename engine-owned C++ APIs to the Hyperion namespace, PascalCase identifiers, and UE-style type prefixes; rename owned source files to PascalCase with .h headers.
- Adopt Allman braces, explicit control-flow braces, and tabs displayed as four columns.
- Record repository-specific rules and interoperability exceptions in contributor guidance and enforce formatting/naming with checked-in tooling.
- Update build inputs, scripts, tests, and current documentation together; preserve runtime configuration and third-party interfaces.

## Capabilities

### New Capabilities

- `code-style`: Default Unreal-inspired source conventions and repeatable style checks.

### Modified Capabilities

None. Runtime requirements remain unchanged.

## Impact

All owned C++ and shader sources, public headers, tooling filenames, CMake integration, and contributor documentation. No dependency versions or vendor sources change. Existing consumers must adopt the renamed public APIs and file paths.
