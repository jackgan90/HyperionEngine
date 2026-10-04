## Why

The documented 500-line limit for owned non-test C++ files currently depends on manual review. A repeatable check should reject new oversized files and growth of existing oversized files while keeping their incremental cleanup visible.

## What Changes

- Add a Python-only physical file size check for owned `.cpp`, `.h` and C++ `.inl` files under `Source`.
- Record existing oversized production files with their current line counts and follow-up responsibilities. Reject growth and require the record to shrink or disappear as files are reduced.
- Use exact, reviewed exclusions with purpose and ownership evidence for tests, generated sources or third-party sources; names and directory names alone do not grant exemptions.
- Expose the check through `CheckStyle.py`, run it from CTest, and add focused checker regressions.
- Update verification guidance while retaining manual function-length exceptions and semantic review.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `code-style`: Add automatic physical file size verification and a reviewed, bounded migration policy for existing oversized files.

## Impact

Changes are limited to Python tooling, its policy data and tests, CTest registration, and development documentation. The checker requires neither LLVM nor a configured C++ build. Runtime code, public APIs, persistence, plugin composition and the 100-line function policy remain unchanged.
