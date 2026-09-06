## Context

The three failures are reproduced in out/ArchitectureReview/Probe.log. The owner limits this change to their fixes, Viewer/importer decomposition and the 100-line coding principle.

## Goals / Non-Goals

**Goals:** recover a healthy device after a failed frame; keep peer windows usable; permit asset retry; split orchestration into readable stages; preserve current CLI, plugin, import and rendering behavior.

**Non-Goals:** event-routing redesign, general render resources/bindings, cache budgets/hot reload, task-system redesign, pipeline caches, unified reflection, and unrelated long functions.

## Decisions

- Balance SDL video/events subsystem initialization using a private RAII owner per window. SDL subsystem reference counts retain video until the last owned reference is released. This fixes global shutdown without introducing a new public platform API.
- Add idempotent `IRHISwapchain::CancelFrame`, invoked on RHI 0 after all recorders finish. Unsubmitted frames release their retained packets; submitted work must drain before releasing resources or clearing frame state. A GPU/device failure may prevent recovery and remains an error.
- Graph execution catches dispatch, recording and end-frame errors, joins all admitted recorders, and cancels before rethrowing the original error. Allocate bookkeeping before acquiring the frame.
- Under the existing asset-cache mutex, reuse pending/successful results and remove a completed failed entry when a fresh load arrives. Existing request objects retain their terminal error; concurrent retries still share one new producer. This avoids introducing a broader cache architecture.
- Split Viewer into private options/verification, application lifecycle, frame operations and capture/save helpers. Explicit lifecycle ownership preserves service order and file-producer draining on normal and exceptional exit.
- Split importer into private accessor/URI utilities, primitive conversion and scene import stages. Keep parsed cgltf data and external buffers owned for the whole import and preserve current validation order.
- Document 100 lines as a review principle, not an unconditional automated failure. Count the complete definition, including comments, blank lines and lambdas. Tightly coupled exceptions require a local explanation. Existing unrelated long functions are deferred.

## Risks / Trade-offs

- Frame cancellation after submission can wait for the GPU -> used only on failure, with existing fence/error handling.
- Extracted helpers may accidentally alter lifetime/order -> preserve regression coverage, exercise CLI variants and RenderDoc on/off builds.
- Failed requests retry only upon a fresh load -> existing consumers keep stable errors and explicit retry semantics.

## Migration Plan

Update the concrete and mock swapchain providers together, rebuild Debug/Release, run regression suites and style/boundary checks. No serialized migration or dependency update.

## Open Questions

None for this bounded change.
