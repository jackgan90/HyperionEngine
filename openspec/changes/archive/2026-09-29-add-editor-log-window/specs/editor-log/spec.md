## ADDED Requirements

### Requirement: Complete current-process log history
Editor SHALL collect Debug, Info, Warning and Error from entrypoint initialization through orderly service shutdown, independently of panel visibility. History SHALL preserve owned source, time, thread, level and monotonically increasing sequence metadata, and SHALL support bounded reads of earlier disk-backed entries without dropping history when a display cache fills.

#### Scenario: Late opening and concurrent output
- **WHEN** startup and worker threads write logs while the panel is closed
- **THEN** opening the panel permits reading startup and subsequent records in sequence without duplication or cache-driven loss

#### Scenario: Current run isolation
- **WHEN** a previous run has appended to the persistent text log
- **THEN** the new Editor panel displays only records from the current run

### Requirement: Standard process output capture
Editor SHALL include its stdout/stderr output with source metadata, using Info for unstructured stdout and Error for unstructured stderr. It SHALL preserve externally redirected output, partial writes, multiline text and final unterminated tails and SHALL avoid recapturing structured log forwarding.

#### Scenario: Redirected run and shutdown tail
- **WHEN** a parent captures Editor output and the process writes native/CRT output plus an unterminated tail
- **THEN** the parent receives the original output and history receives each captured fragment once after orderly draining

### Requirement: Dockable Editor Log window
Window > Log SHALL toggle the panel and reflect its state. The panel SHALL be initially hidden, remain draggable like Content Browser and Place Object, and initially share the bottom Content Browser dock node. Closing the panel SHALL NOT clear history or stop collection.

#### Scenario: Existing layout upgrade
- **WHEN** a user opens Log with a saved layout containing no Log window
- **THEN** Log receives a bottom placement without resetting other windows

#### Scenario: User placement and reset
- **WHEN** a user drags Log, closes/reopens it or restarts with the saved layout
- **THEN** its saved placement is respected, and Reset Layout restores the default bottom tab arrangement

### Requirement: Readable colored history
The panel SHALL show Error in red, Warning in yellow and Info/Debug in white, preserve log text rather than applying content-path presentation rewriting, and render only visible rows. New output SHALL follow the tail only while the view is already at the bottom.

#### Scenario: Read earlier multiline messages
- **WHEN** a user scrolls upward while new multiline messages arrive
- **THEN** the view stays on the earlier content and each displayed message line retains its severity color

### Requirement: Console-free Editor startup
The Editor executable SHALL use the Windows GUI subsystem without allocating a default console, while retaining its arguments, redirected diagnostics and exit status. Other CLI executables SHALL keep their console behavior. Interactive startup failures SHALL remain visible; hidden, redirected or bounded runs SHALL NOT wait on a modal error dialog.

#### Scenario: Failed automated startup
- **WHEN** a hidden redirected Editor run fails before creating its main window
- **THEN** the parent receives an error diagnostic and a nonzero exit status without a blocking dialog

### Requirement: Shared typed automation reads
Attached automation SHALL expose discoverable, reflected, bounded log reads backed by the same history used by the panel. Invalid cursors or limits SHALL fail without mutating history. Missing history SHALL return unavailable and adapter cleanup SHALL precede provider destruction.

#### Scenario: Discover and resume reads
- **WHEN** an agent discovers and describes application.log.read then reads successive pages
- **THEN** the pages preserve sequence, severity, source and text and permit continuing from the returned cursor

#### Scenario: Provider absent
- **WHEN** the log operation is invoked without an active history provider
- **THEN** it reports unavailable without affecting unrelated automation operations
