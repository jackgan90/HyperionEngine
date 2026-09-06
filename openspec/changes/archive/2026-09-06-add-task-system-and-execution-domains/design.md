## Context
Core services are complete. The renderer requires stable dedicated thread identities, while general CPU work must use oneTBB through an internal wrapper.

## Goals / Non-Goals
**Goals:** dependency-aware dispatch, waits, errors, counters and shutdown with configurable workers and RHI threads.
**Non-Goals:** GPU fence semantics, automatic detection of arbitrary user-created wait cycles, or executing generic worker tasks on dedicated threads.

## Decisions
- Keep Main as the creating thread. Render and RHI use private queues and dedicated threads; Main explicitly pumps its queue.
- Schedule workers in oneTBB's task_arena with bounded concurrency. TaskState and continuation lists remain engine owned and use the tagged Tasks allocator.
- Register dependency continuations without occupying worker threads. Failed dependencies prevent dependent bodies from executing and propagate their error.
- Use oneTBB resumable tasks for waits inside workers, permitting a one-worker nested task to make progress. Dedicated threads use completion notifications; same-domain incomplete waits are rejected except Main, which pumps its own queue.
- Shutdown closes external admission, pumps Main until admitted work completes, then joins dedicated threads. It must be invoked on the Main owner thread. Tasks must not create new work after shutdown begins; callers should finish producer tasks first.
- Tracy scopes must not span resumable-task waits because suspended tasks can resume on another worker. Executor counters provide cross-task observability independently.

## Risks / Trade-offs
- Cyclic synchronous waits across dedicated domains → document acyclic waits and prefer explicit dependencies.
- Last-completion races → protect continuation registration and completion with the same state mutex.
- Scheduler backend lifetime → drain admitted tasks before releasing the arena or queues.
