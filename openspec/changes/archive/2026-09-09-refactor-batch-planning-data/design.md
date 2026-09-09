## Context

The current planner verifies the same reflection, item state and instance values in item description, chunk lookup, record packing and plan capture. Large camera motion changes visible membership and repeatedly pays these costs even when most source items are unchanged. Trace evidence is recorded in the proposal; baseline binaries and hashes are frozen under `out/batch-planner-refactor-20260909`.

## Goals / Non-Goals

Goals: reduce metadata reconstruction on real planning paths; preserve full equality proofs, custom strategy evaluation, exact coverage, immutable publication and cache limits; validate both stable and changing inputs in Debug and Release.

Non-goals: shader ABI changes, persistent GPU scene slots, reducing validation, changing visible work, or optimizing group hashing without residual measurements.

## Decisions

1. Compile the immutable program/pass instance contract once into a bounded, weakly associated cache. It references the immutable compiled program/pass contract and extracts effective instance parameter indices once. Canonical candidate structures already cover complete physical layout equality; chunks no longer duplicate those layouts. Reflection and string/layout construction move outside per-item and per-chunk hot loops. Program pointer equality is valid only while the original weak owner is live.
2. Prepare stable item instance proofs once and share them between plan and chunk caches. Reuse requires the complete source generation/local identity, live lifetime owner, geometry/material resource epoch, section and effective state. Each instance value proof retains only its weak owner and address. Matching the current strong value's complete owner/address identity is sufficient; semantic comparison of a different value object first locks the old weak owner. An expired nonempty proof never equals null, and cached value addresses are never dereferenced. A private Render-only cursor tracks the latest state/local pages separately from immutable source/value proofs, so new resolved objects and state changes can reuse identical instance bytes after regrouping. Local-page proofs store weak owners plus page revisions and overlay masks; they neither retain textures nor miss in-place page writes. Shared overlays that touch instance fields use the same complete comparison; no assumption about public caller overlays is permitted.
3. Keep count-bounded planning proofs separate from byte-bounded payload storage. Preparations own no numeric trees and are limited by MaxItems, like candidate/plan metadata; CachedInputs and CachedInputBytes expose their count and structural metadata estimate (excluding map/list nodes, allocator overhead and weakly retained make_shared allocations). Compact instance chunks use half the configured MaxBytes, and the existing record/block packer uses the other half. Plans/chunk member lists retain only weak preparation references. This avoids making stable plan validity depend on the byte cache: independent audit found the prior shared planning pool still thrashed for default Debug 2048-item input and small-budget ordinary fallbacks. Raising a fixed partition would only move that threshold. Existing candidate retirement removes corresponding dead preparations; a cursor sweeps at most 64 orphan preparations per family. Insertion enforces the count limit independently and rolls back its LRU node if map insertion throws. Preserve conservative behavior for arbitrary strategies. Anonymous inputs do not establish persistent proofs.
4. Preserve the existing record/block packer unless measurement justifies an additional change. It runs on changed chunks only; reducing repeated hot-loop gathering has higher measured value than replacing the GPU payload representation. Ordered membership and complete values remain the final correctness authority.
5. Add a repeatable benchmark of the real planner using initialized engine materials, outside GPU submission timing. Exercise visibility, order, values, resource/state splits and view changes. Freeze the benchmark before implementation. Validate coverage and workload outside measured regions and report regressions alongside gains.

## Risks / Trade-offs

- Incorrect identity reuse → weak owner checks, complete state/effective value comparisons and generation/address-reuse tests.
- Shared overlays can contain instance values → contract-aware value validation and regression tests for this public case.
- Moving snapshots into shared preparations can retain history → bound metadata and payload caches; plans hold weak references where possible; test retirement and in-flight lifetime separately.
- Indirection can regress stable plans → keep direct unchanged-input paths and measure stable, small-motion and large-motion workloads in both build modes.
- Custom strategies may depend on arbitrary values → preserve complete conservative input validation and reevaluation.

## Migration Plan

Create and freeze a real-planner benchmark, implement reusable contracts and prepared inputs in focused stages, run cache/coverage/lifetime tests, then compare frozen binaries on identical benchmark and SceneViewer inputs. Retain only changes supported by measurements. Existing flags, native contracts and executable names remain compatible; rollback is an internal source revert.

## Open Questions

The degree of record-packer integration and additional grouping changes is decided from post-refactor profiles, rather than assumed in advance.
