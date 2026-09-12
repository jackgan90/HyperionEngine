## ADDED Requirements

### Requirement: Scene publication is bound to frame admission
Scene-bound frame input SHALL carry an owned publication token including logical scene identity, attachment epoch and publication serial. Normal Main admission SHALL publish scene updates and enqueue that frame before publishing a later scene state. Render SHALL verify an exact match to its applied publication before resolving camera, lights and geometry for the frame. A mismatched, foreign or failed publication SHALL fail explicitly before scene draw generation rather than substituting the latest available state. The token SHALL NOT require unbounded historical scene retention or synchronous GPU completion.

#### Scenario: Delayed Render with later Main edits
- **WHEN** Render is gated while Main admits frame N and edits/admit frame N+1
- **THEN** Render processes publication N and frame N before publication N+1 and frame N+1, preserving each frame's corresponding model, camera and light data

#### Scenario: Caller queues publications out of the frame contract
- **WHEN** a caller freezes an old token, applies a newer publication and then requests the old scene frame
- **THEN** Render reports ScenePublicationMismatch without rendering a mixture or silently selecting the newer publication

#### Scenario: Reattach the same logical scene
- **WHEN** an old token is reused after the same FScene attaches again
- **THEN** the changed attachment epoch rejects it even if other identifiers or revision values appear equal

### Requirement: Scene control progress and old-frame lifetime remain independent of presentation
Scene camera/light/model updates, metadata removal and failure observation SHALL progress on the ordered control queue during minimized or skipped-drawing ticks. Closing a scene SHALL clear its Render metadata and join admitted CPU work, including after publication failure. Already prepared frames SHALL retain their original immutable data and existing GPU leases, with no new scene-specific GPU idle waits.

#### Scenario: Minimized camera and light edit
- **WHEN** camera/light edits and removals occur while drawing is skipped
- **THEN** the scene publication progresses and the next valid frame reflects those changes without stale selection or lighting

#### Scenario: Close with delayed RHI work
- **WHEN** an old prepared graph is retained while scene nodes are removed or the scene is closed
- **THEN** that graph remains valid through its existing owners and new scene frames do not inherit removed metadata
