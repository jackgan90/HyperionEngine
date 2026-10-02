## ADDED Requirements

### Requirement: Equivalent ordinary and cached native draw interpretation

D3D12 ordinary recording and cached draw-plan construction SHALL share native geometry, dynamic and indexed-draw value interpretation. Cached payloads SHALL express their operation-specific values and bind statistics by named typed fields without relying on generic positional integer arrays or void pointer recovery. Existing supported format, signed offset, first index, counts, stride and dynamic-state semantics SHALL remain unchanged; native details SHALL stay private to the backend.

#### Scenario: Nondefault indexed draw parameters
- **WHEN** a valid packet has nonzero FirstIndex, a legal negative VertexOffset and a nondefault InstanceCount
- **THEN** ordinary, first owned and reused owned paths submit the same independently expected native draw arguments and rendered output

#### Scenario: State switching
- **WHEN** a pass switches pipeline, root/resource bindings, geometry, scissor, blend and stencil state
- **THEN** both paths preserve expected state changes and bind suppression with identical output and named statistics

### Requirement: Cached draw admission preserves ownership and validation

Draw-plan refactoring SHALL preserve shell, target, access and ownership validation, first-miss ordinary recording, later plan construction/reuse, constant-page registration before validation and existing fenced recorded-command retention. Plan caches SHALL NOT become strong resource owners. New immutable streams or changed target/access contracts SHALL invalidate incompatible reuse while previously recorded streams remain valid.

#### Scenario: Cold build and reuse
- **WHEN** an immutable shared draw stream is recorded repeatedly with compatible targets and accesses
- **THEN** its first miss uses validated ordinary recording, its next matching recording builds a plan and later recordings reuse that plan without rebuilding or fully revalidating every draw

#### Scenario: Nested constant page and expired stream
- **WHEN** a stream holds the sole nested constant-page handle and later all commands and fenced work are retired
- **THEN** reset remains blocked while commands are live and becomes possible after they expire, independently of cache lifetime

#### Scenario: Incompatible shell or new stream
- **WHEN** a cached shared stream is submitted through malformed shell storage or incompatible target/access data, or draw arguments change in a new stream
- **THEN** invalid submissions are rejected before execution and the new stream obtains its own validated preparation without changing old immutable commands

### Requirement: Measured native draw-plan maintenance

Changes to shared draw interpretation or cached payload representation SHALL have comparable Release baseline and final measurements for ordinary and owned paths, homogeneous and real state-switch workloads, command storage and cold/build/reuse behavior. Measurements SHALL preserve fixed settings, multiple trials and raw evidence. Repeatable performance or storage regressions SHALL be investigated and resolved or explicitly left unaccepted; a single timing result SHALL NOT establish equivalence.

#### Scenario: Payload refactoring acceptance
- **WHEN** a cached command representation is changed
- **THEN** review receives before/after command size/count/capacity, lifecycle evidence and median/p95 recording times with trial variation for 0, 1, 100, 300, 600 and 1200 draws
