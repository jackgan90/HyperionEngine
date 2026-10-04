## Context

`CheckStyle.py` discovers owned sources and checks paths, formatting and optional semantic naming. CTest runs only its paths mode. File and function length currently rely on review, and existing non-test files exceed the documented 500-line limit.

## Goals / Non-Goals

**Goals:**
- Enforce physical file size without LLVM, a compile database, Git history or a configured build.
- Bound existing debt by exact file and line count, with actionable cleanup responsibilities.
- Keep test exclusions explicit and reviewable instead of guessing from names.
- Exercise the checker and its command-line integration through CTest.

**Non-Goals:**
- Parse C++ functions or change their tightly coupled exception.
- Split production code, alter runtime behavior or infer semantic maintainability from tool success.
- Build a general CMake interpreter or automatically rewrite the policy.

## Decisions

### Python-only scan with one shared implementation

Add `tools/SourceSize.py` and call it from the normal `CheckStyle.py` flow and a new `--sizes-only` mode. Preserve `--paths-only` behavior. Read-only modes check size before LLVM lookup; `--format` checks size after formatting writes so its result covers the final files. The size-only mode is read-only and rejects combinations with formatting/naming options. CTest registers a size check and its Python regressions separately from the existing path check.

Scan all `.cpp`, `.h` and `.inl` files under `Source`, regardless of enabled build options. Count LF physical lines, accepting CRLF and an unterminated final line; blank lines and comments count, while control characters inside a line do not create extra lines. Files outside `Source`, including `out/deps` and generated build outputs, are outside owned-source discovery. Exact exclusions cover any reviewed third-party or generated C++ placed inside `Source`.

### Explicit policy separates legacy debt from exclusions

`tools/SourceSizePolicy.json` contains two exact-path maps:
- `legacy_files`: current physical `lines` and the logical responsibility to `split` later.
- `exclusions`: `kind` (test/generated/third_party), `reason`, and `evidence` identifying build ownership or the generator/vendor source.

Unclassified files use the 500-line limit, even when their name contains Tests or their directory is Source/Tests. Only files needing an exclusion must be classified; smaller tests can remain conservatively checked. Current large tests are reviewed against their source purpose and CMake registrations. The tool validates policy shape, duplicate keys, exact existing source paths and mutually exclusive classification. It does not prove that an exclusion's human-reviewed purpose or build ownership is correct; changes to those records require source/build review. A production implementation reused by a test target remains production.

This is preferred to directory/suffix heuristics, which can silently exempt production code, and to a generated build inventory, which would require configuration and miss disabled sources.

### Existing debt ratchets down

Initial entries record the existing oversized production files. Growth fails. If a file shrinks but remains above 500 lines, the check requires lowering its recorded count; at 500 lines or below it requires removing the entry. Deleted or renamed policy paths fail until reviewed policy cleanup is complete. A rename can transfer its existing ceiling and splitting responsibility during review; new oversized production files or raised ceilings must not be added to silence the check.

The checker never updates policy. Review checks that policy changes only reduce debt or account for verified renames, and that test/generated/vendor exclusions have real evidence. Enforcing historical policy edits without a trusted reference is outside the standalone checker; changing policy remains a visible review action.

### Verification scope

Use temporary synthetic source trees to test the 500/501 boundary, final newline and CRLF, source discovery, exact exclusions, baseline growth/reduction/removal, malformed policy and CLI failure propagation. Temporary fixtures live under ignored `out` during repository tests. Reconfigure the existing CMake build and run the new CTest entries plus path and module-boundary checks. No production C++ is modified, so full renderer/GPU regression is unnecessary.

## Risks / Trade-offs

- Manual exclusion evidence can become stale → require review when purpose/build ownership changes, reject missing source paths and avoid automatic name-based exemptions.
- A contributor could edit the policy to relax checks → never auto-generate exemptions; review policy diffs with the source change.
- Strict line-count refresh adds a small maintenance step → diagnostics explain whether to lower or remove an entry.
- File-size success does not establish architectural quality → documentation retains manual function, ownership and semantic review.

## Migration Plan

Land the checker, reviewed policy, focused tests and documentation together. Existing production files remain intact and are split in later focused changes. Reverting this tooling change restores manual enforcement without runtime or data migration.
