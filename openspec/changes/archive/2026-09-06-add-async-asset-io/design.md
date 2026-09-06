## Context

Asset libraries currently open files directly and there is no IO executor. Engine-owned byte IO is required before asynchronous model loading can control storage, ownership and shutdown.

## Goals / Non-Goals

Goals: Add one IO execution domain to FTaskSystem without changing existing domain names or queue ownership. Add Runtime/IO with local and injectable storage, bounded owned byte reads, atomic writes, cancellation and statistics. Separate physical IO from worker decoding; retain explicit synchronous bootstrap/test compatibility.

Non-goals: no implementation of future FBX/OBJ/COLLADA codecs, animation, compressed glTF extensions, full asset cooking, ECS or additional graphics backends in this series.

## Decisions

### 1. Decision

Add EDomain::Io as a dedicated executor, keeping Main/Render/RHI indexes stable. FIOService schedules only storage operations there; CPU tasks use existing dependency/suspend support. One local blocking IO thread is the initial backend, not a concurrency promise.

### 2. Decision

IFileSystem reads/writes owned byte buffers; FLocalFileSystem is the physical adapter and FMemoryFileSystem supports deterministic tests. Reads have byte limits. Paths are std::filesystem::path; URI decoding belongs to importers. Writes use a unique same-directory temporary and replace operation.

### 3. Decision

TAsyncResult<T> pairs a task handle with shared result state. DispatchAsync captures the caller's shared cancellation token and checks it before work and publication. Ready results can be queried without waiting; Get delegates waiting to FTaskSystem. An OS read/write already in progress may finish before cancellation is reported; cancellation cannot undo a committed replacement.

### 4. Decision

FIOService must outlive its requests. Asset producers drain before Tasks.Shutdown. Startup configuration, diagnostics and explicit legacy synchronous wrappers are compatibility exceptions; new asset and Viewer save paths default to asynchronous IO.

## Risks / Trade-offs

Single blocking reads serialize device access; measure queue latency before adding overlapped IO. No CPU work or task waits are permitted inside the IO executor. Exceptions and limits must release owned buffers.

## Migration Plan

Keep existing target names, serialized keys, triangle and configuration tests working. Each change is additive until its replacement paths are verified; retain explicit compatibility wrappers where required. Validate focused tests before continuing and run full Debug/Release verification at the end.

## Open Questions

No blocking product decisions. Implementation refinements must be reflected here and validated before task completion.
