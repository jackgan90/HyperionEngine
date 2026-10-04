## Context

Baseline `05d572c` uses a seven-field FBatchItemKey, five-field instance record key and four-field resource-retry key. Ordered batch retirement depends on the view/usage prefix; resource retry lookup depends on identity/version/configuration preceding retry generation.

## Goals / Non-Goals

Goals: named identity fields and operations, exact preservation of equality/hash/order/retry behavior, unchanged source leases and GPU lifetime.

Non-goals: cache optimization, key dimension changes, generalized cache frameworks, unrelated tuple cleanup or MaterialConstantCache splitting.

## Decisions

- Define focused private records for the three key domains, using named fields and designated construction at multi-field call sites.
- Preserve the original lexicographic field order and hash mixing order. Provide a named batch view boundary and membership query rather than external positional prefix interpretation.
- Resource retries retain their identity/version/configuration prefix and unsigned retry counter semantics. Expose same-request and retry progression operations. Keep pointer ordering consistent with the existing standard-library tuple comparison.
- Keep the instance record key local to its cache domain; expose it only through a private header for independent key-contract tests if needed.
- Validate against a frozen legacy tuple model, vary every identity dimension, and use existing batch/resource GPU tests to cover reuse, invalidation, old-frame retention and failures.

## Risks / Trade-offs

- Field ordering changes affecting lower_bound -> compare new ordering and view ranges against the old tuple model, including different views/usages and depth conventions.
- Hash/equality drift -> independently compute legacy hashes and verify every field participates exactly as before.
- Pointer order changes -> preserve tuple-based ordered comparison internally where required; positional access remains absent from consumers.

## Migration Plan

Migrate keys and direct consumers atomically. Run focused key tests plus existing batch/resource suites in Debug and Release. No persisted data migration; leave active artifacts and unstaged implementation for user acceptance.
