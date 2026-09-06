# task-execution Specification

## Purpose
TBD - created by archiving change add-task-system-and-execution-domains. Update Purpose after archive.
## Requirements
### Requirement: Explicit execution domains
The task system SHALL run tasks on their selected Main, Render, indexed RHI or general Worker domain.
#### Scenario: Dedicated routing
- **WHEN** tasks are submitted to each dedicated domain
- **THEN** each domain executes on its own stable thread and Main remains the creating thread

### Requirement: Dependencies and errors
Dependent tasks SHALL start only after all prerequisites complete successfully; failures SHALL propagate to waiters and dependents.
#### Scenario: Failed prerequisite
- **WHEN** a prerequisite throws
- **THEN** the dependent body is skipped and waiting on its handle reports the error

### Requirement: Nested worker progress
Waiting on child tasks from a Worker SHALL allow pending Worker tasks to execute even with one Worker.
#### Scenario: Single worker nested wait
- **WHEN** a Worker dispatches a child Worker task and waits for it
- **THEN** both tasks complete without creating an additional engine worker pool

### Requirement: Complete shutdown
Owner-thread shutdown SHALL drain admitted tasks and join dedicated threads before releasing scheduler state.
#### Scenario: Queued Main completion
- **WHEN** shutdown begins with admitted tasks targeting Main
- **THEN** Main work is pumped and all admitted completion handles become ready
