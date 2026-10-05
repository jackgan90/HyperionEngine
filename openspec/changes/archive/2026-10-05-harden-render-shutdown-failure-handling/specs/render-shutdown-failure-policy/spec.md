## ADDED Requirements

### Requirement: Observable explicit Close and transient recovery

Explicit session/resource Close SHALL propagate cleanup failure without claiming successful closure. A caller SHALL be able to retry a transient native cleanup failure; successful Close SHALL remain idempotent. Normal cleanup MUST join captured CPU work and retire native resources on RHI 0 only after completion is established.

#### Scenario: One-time idle failure

- **WHEN** explicit Close encounters one injected idle failure and the next attempt succeeds
- **THEN** the first failure remains observable and subsequent Close/destruction complete normally without double release

### Requirement: Contain unrecoverable destructor failure

If session/resource Close fails during destruction, the destructor SHALL contain the exception and invoke a shared terminal policy. That policy SHALL record owner, stage, reason and immediate-process-exit outcome, then exit with EXIT_FAILURE without C++ member/atexit unwinding. It MUST NOT mark native retirement successful, fabricate GPU completion or release retained native state after a failed wait.

#### Scenario: Persistent idle failure during plugin Stop

- **WHEN** the graphics plugin catches explicit Close failure and destructor retry encounters the same persistent device idle failure
- **THEN** the process reports the original failure and terminal policy and exits unsuccessfully
- **AND** neither std::terminate nor native owner destruction through member unwinding occurs

#### Scenario: Persistent collection failure in resource destruction

- **WHEN** native completed-resource collection persistently fails during resource-service destruction
- **THEN** the same terminal policy records the resource-service owner and exits unsuccessfully without pretending resources are retired

#### Scenario: Terminal logging fails

- **WHEN** the normal terminal log cannot be recorded because logging throws
- **THEN** a minimal stderr record preserves failure facts before immediate unsuccessful exit
