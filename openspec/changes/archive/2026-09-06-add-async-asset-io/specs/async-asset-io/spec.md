## ADDED Requirements

### Requirement: Dedicated byte IO
The engine SHALL route asset file reads and writes to one engine-owned IO executor and expose owned byte buffers without vendor types.

#### Scenario: Dedicated byte IO acceptance
- **WHEN** a worker requests a file from an instrumented backend
- **THEN** the backend executes on the IO thread and subsequent decoding executes on Worker

### Requirement: Failure and cancellation
The engine SHALL bound read sizes, propagate storage failures, and prevent a canceled request from publishing a successful value.

#### Scenario: Failure and cancellation acceptance
- **WHEN** a read exceeds its limit or is canceled
- **THEN** the caller receives a terminal error and no partial buffer

### Requirement: Atomic persistence
The engine SHALL replace a destination only after its complete temporary output is written.

#### Scenario: Atomic persistence acceptance
- **WHEN** a destination is overwritten
- **THEN** readers observe a complete old or new file and temporary files are cleaned up
