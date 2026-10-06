## ADDED Requirements

### Requirement: Domain-independent local derived data
A reusable runtime cache SHALL store disposable immutable payloads by validated namespace/key, independently of application builds and content mounts. Producers SHALL own semantic key construction including generator/input compatibility. Cache records SHALL verify format, length and payload integrity before returning a hit.

#### Scenario: Damaged record
- **WHEN** a stored payload is truncated, corrupt or uses an unsupported format
- **THEN** lookup reports a miss and the producer can regenerate it

### Requirement: Concurrent atomic publication and recoverable failure
Cache publication SHALL expose only complete records across threads/processes. Failed cache reads/writes SHALL NOT invalidate an otherwise successfully generated artifact. Errors from the actual producer SHALL remain errors.

#### Scenario: Simultaneous producers
- **WHEN** two processes publish the same semantic key
- **THEN** readers observe either a miss or a complete validated record

#### Scenario: Unwritable cache
- **WHEN** a shader compiles successfully but the cache cannot publish it
- **THEN** the compilation result remains usable and the storage failure is reported without high-frequency repeated logging

### Requirement: Confined bounded maintenance
Cache maintenance SHALL be restricted to recognized cache records in its namespace, use explicit age/size policy, and bound work per invocation. It SHALL NOT remove user configuration, captures or authored content.

#### Scenario: Cleanup boundary
- **WHEN** maintenance removes expired records
- **THEN** unrecognized files, linked directories and files outside the namespace are not deleted
