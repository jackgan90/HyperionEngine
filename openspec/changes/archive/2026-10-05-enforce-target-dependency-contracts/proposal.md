## Why

The boundary checker treats all target tokens in a CMake file as dependencies of its first production target. Test links can therefore hide missing direct dependencies or create false cycles, and public header dependencies are not checked for visibility. Architecture refactoring requires a trustworthy configured baseline that identifies each compilation owner and distinguishes direct public dependencies from incidental transitive exports.

## What Changes

- Check configured target/source ownership and expanded direct CMake link declarations, keeping production and test graphs distinct.
- Validate direct dependencies, public visibility, actual cycles, CPU domain isolation including Environment, and existing private/vendor boundaries with actionable locations.
- Reject unsupported or stale metadata explicitly; document configuration coverage.
- Correct verified dependency declarations and compile small consumers that link only the public target under test.
- Add checker fixtures and an overview of current architecture ownership.

## Capabilities

### New Capabilities
- `target-dependency-contracts`: configured build graph and source boundary validation with explicit coverage and regression fixtures.

### Modified Capabilities
- None. Existing runtime behavior and public contracts remain unchanged.

## Impact

Build configuration/validation helpers, `tools/CheckBoundaries.py`, focused test fixtures and public consumers, verified module link declarations, and architecture documentation. Current RHI-to-Assets links remain allowed until the image-data migration. No engine feature, provider, serialized identifier, executable name, or lifecycle changes.
