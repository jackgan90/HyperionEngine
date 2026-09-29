# engine-diagnostics Specification

## Purpose
Define contextual, low-noise engine diagnostics at shared lifecycle and operation boundaries, including native validation and shader compilation diagnostics exposed through the existing log history.
## Requirements
### Requirement: Contextual terminal diagnostics
Engine services SHALL log critical plugin startup/cleanup, content transitions, import completion, document save completion and scene load/refresh failures at their owning boundary. Messages SHALL identify the operation and relevant path or identity, preserve the failure cause, and state a factual outcome or recovery action.

#### Scenario: Failed asynchronous import
- **WHEN** an accepted import fails
- **THEN** an Error record identifies its task, source, output and failure cause while the task retains its existing failed result

#### Scenario: Recovery and cleanup
- **WHEN** a saved content root cannot be restored or plugin cleanup throws
- **THEN** the log identifies the affected root or plugin and the recovery/continuation behavior without interrupting remaining cleanup

### Requirement: Quiet ordinary execution
New diagnostics SHALL NOT log ordinary frame, draw, object-update, cache-hit or status-poll activity. Repeated polling of a completed or failed async operation SHALL NOT emit duplicate completion/failure records. Expected cancellation SHALL remain distinct from failed work.

#### Scenario: Repeated terminal polling
- **WHEN** a failed or completed import/save/scene is polled repeatedly
- **THEN** its terminal diagnostic is not emitted again

### Requirement: Native validation visibility without replay
D3D12 SHALL expose native Warning, Error and Corruption records with the native message ID and description, suppress identical severity/ID/text repeats per device, and preserve existing validation error statistics.

#### Scenario: Repeated statistics and messages
- **WHEN** statistics are queried repeatedly and identical native validation records are added
- **THEN** the log contains one record per distinct severity/ID/text while error counts still reflect all stored errors

### Requirement: Shared diagnostic access
GUI and automation executions SHALL use the same domain diagnostics and existing log history/read operation. Existing exception, result, revision, persistence and cancellation contracts SHALL remain intact.

#### Scenario: Agent consumes engine failure
- **WHEN** an attached agent reads application.log.read after a domain failure
- **THEN** it can retrieve the same contextual failure record displayed in Editor Log

### Requirement: Shader compilation diagnostics
Actual shader compilation SHALL report diagnostic text with source, entry, profile, payload target and compile options, including Warning records for successful compilation with warnings. Cache hits SHALL NOT replay these diagnostics.

#### Scenario: Cached shader with warning
- **WHEN** a shader compiles successfully with a warning and is requested again from cache
- **THEN** its warning appears at compilation with identifying context and the cache hit adds no diagnostic
