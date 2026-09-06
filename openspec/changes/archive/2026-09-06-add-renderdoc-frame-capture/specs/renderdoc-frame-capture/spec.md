## ADDED Requirements

### Requirement: Optional engine-owned RenderDoc integration
The engine SHALL provide a build-optional RenderDoc capture wrapper with vendor APIs confined to Runtime/Capture private adapters and a startup plugin selected by the stable ID renderdoc.

#### Scenario: Build without RenderDoc
- **WHEN** HYP_ENABLE_RENDERDOC is disabled
- **THEN** existing applications and tests build and run without a RenderDoc header, import library or DLL dependency

#### Scenario: Runtime disabled
- **WHEN** renderdoc is absent from the requested plugins
- **THEN** the engine does not actively load the RenderDoc runtime

### Requirement: Early runtime activation and safe lifetime
The plugin SHALL initialize before any DXGI/D3D12 device creation, reuse an injected runtime where present, negotiate a supported API, and retain graphics hooks for the process lifetime.

#### Scenario: Direct Viewer startup
- **WHEN** the plugin is enabled with a compatible installed runtime
- **THEN** the ordinary Viewer process can capture without being launched from the RenderDoc UI

#### Scenario: Unavailable runtime
- **WHEN** the configured runtime cannot load or negotiate the required API
- **THEN** rendering continues and the capture controls report why capture is unavailable

### Requirement: Complete frame capture with exclusive ownership
The engine SHALL accept at most one pending capture request and bracket all frame GPU preparation, concurrent recording, submission and presentation with explicit capture control on the RHI coordinator.

#### Scenario: Capture an experiment frame
- **WHEN** a capture is requested in a drawable Triangle or Model Viewer frame
- **THEN** one RDC contains that experiment's rendering and enabled debug UI and can be replayed

#### Scenario: Overlap or external capture
- **WHEN** another request is pending or RenderDoc is already capturing externally
- **THEN** the request is refused or reported busy without overlapping, ending or discarding the external capture

#### Scenario: Failed or skipped frame
- **WHEN** rendering fails during an owned capture or the application closes while a request is pending
- **THEN** the request is cancelled or failed and any owned capture is cleaned up without reporting success

### Requirement: Verified capture output
The engine SHALL discover newly produced capture records, verify a nonempty file exists, and return its exact path and success or failure independently of earlier captures.

#### Scenario: Consecutive captures
- **WHEN** multiple successful requests are issued sequentially
- **THEN** each produces a distinct RDC and the latest successful path is updated

#### Scenario: Capture output failure
- **WHEN** output creation fails
- **THEN** failure is visible and the engine does not report or automatically open an older capture as the new result

### Requirement: Shared interactive capture controls
The diagnostics panel SHALL expose Capture RDC, Open last capture, an opt-in automatic-open checkbox and status for both Viewer experiments, while retaining the existing screenshot action.

#### Scenario: Button interaction
- **WHEN** a user presses and releases an enabled RDC capture button
- **THEN** exactly one capture request is emitted and the resulting status and path appear in the panel

#### Scenario: Unavailable or busy controls
- **WHEN** capture is unavailable or already pending
- **THEN** its button cannot issue another request and the status explains the condition

### Requirement: Optional replay UI opening
The plugin SHALL open the exact latest successful RDC with the matching RenderDoc UI on request or after a successful capture when automatic opening is enabled; automatic opening SHALL default to disabled.

#### Scenario: Automatic opening
- **WHEN** capture succeeds with automatic opening enabled
- **THEN** the engine launches RenderDoc with that capture file, preserving spaces and Unicode in the path

#### Scenario: UI launch or file failure
- **WHEN** launching the UI fails or the last file has been removed
- **THEN** the UI operation reports a distinct error and does not overwrite a successful capture result

### Requirement: Reproducible validation
The repository SHALL provide deterministic capture commands, shared widget input tests, optional real GPU capture/replay acceptance, and documented build-on/build-off validation.

#### Scenario: RenderDoc-enabled acceptance
- **WHEN** the documented enabled Debug and Release validations run on a supported machine with RenderDoc
- **THEN** both experiments generate replayable RDCs, GUI requests work, failure cases are checked, and existing PNG and renderer tests pass
