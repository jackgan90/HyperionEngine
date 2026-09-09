## Why

At revision `5895bd7`, warmed Showcase rendering reuses GPU data but camera motion still refreshes hundreds of CPU material results and invalidates scene/packet preparation. The measured Debug CSM preparation cost grows from 1.52 ms stationary to 26.36 ms with small camera motion; changing visibility additionally rebuilds instance chunks.

## What Changes

- Retain stable object/material evaluation and share changed engine parameter blocks through batch and draw preparation, including generic materials with conservative mixed-dependency fallback.
- Separate stable scene item preparation from view visibility/order changes and bound all retained histories across views and families.
- Reuse independent instance constant blocks and packed records, including compatible data across views, so changing an object does not upload unchanged material blocks.
- Preserve stable draw resource/state preparation while refreshing only changed constants; measure native preparation before deciding whether a new RHI command representation is justified.
- Add counters and regression coverage for update granularity, frozen frames, multiple views, visibility changes and bounded retirement.
- Preserve baseline binaries and publish reproducible Debug/Release static/moving CSM comparisons. Update design/tasks/evidence whenever measurements change the implementation direction. Leave the initial implementation uncommitted for review; archive and commit only after the subsequent user authorization.

## Capabilities

### New Capabilities

- `incremental-render-updates`: End-to-end CPU update granularity for shared engine parameters, retained scene items, independent instance data and measured moving-camera performance.

### Modified Capabilities

None. Existing material, ordering, RHI validation, ownership and extension contracts remain in force; the new capability adds stronger incremental-work requirements.

## Impact

Renderer material evaluation, scene preparation, batch caches, draw/constant preparation, integration benchmarks and tests. RHI changes are conditional on measured remaining cost. No new third-party dependency, application-specific material fast path, reduced CSM workload, weakened Debug/validation settings or mutable in-flight uploads.

## Delivered Outcome

The delivered changes retain the current native/shader ABI and add shared/local material separation, stable owning scene-item storage, bounded independent view history, complete immutable batch structure/value signatures, packed records and independent CPU/GPU instance blocks. Repository C++ callers were updated for the new snapshot-list, block-byte and signature representations; custom collection and authored assets retain their contracts.

Final moving-camera CPU preparation improves 35.8%/32.6% for Debug small/large CSM motion and 37.0%/36.9% for Release. Initial Debug engineering targets were not achieved; static and whole-frame regressions, remaining costs, intermediate failed ideas and all validation are explicitly documented in [implementation.md](implementation.md) and [the full report](../../../../docs/IncrementalRenderUpdates.md). The initial delivery was uncommitted; the user subsequently authorized specification sync, archive and Git commit on 2026-09-09.
