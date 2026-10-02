## MODIFIED Requirements

### Requirement: Shared render and visibility controls
Editor SHALL expose validated pipeline, GBuffer, clustered lighting, culling, frozen culling, batching, model bounds and light influence controls using UI-independent services shared with Automation. Transient view/debug state SHALL remain separate from scene history. Directional/contact shadow authoring SHALL use light component transactions. Legacy rendering defaults SHALL remain compatible. Pipeline and GBuffer controls SHALL consume the shared raster option identities and update only their own changed fields; unrelated settings edits SHALL preserve the selected options. Existing operation IDs, versions, wire shapes and revision validation SHALL remain unchanged.

#### Scenario: Change visibility diagnostics
- **WHEN** a user or agent changes culling or batching and freezes the culling view
- **THEN** subsequent rendering uses those settings while display navigation and shadow views continue using the current camera

#### Scenario: Debug overlay isolation
- **WHEN** bounds or influence visualization is enabled in a resized docked viewport
- **THEN** geometry is projected and clipped to that viewport and does not change saved scene data

#### Scenario: Edit an unrelated setting
- **WHEN** the user changes VSync while any supported pipeline and GBuffer preset is selected
- **THEN** the shared service receives the same pipeline and GBuffer values and saving/reopening preserves them
- **AND** rearranging the displayed choices does not change their stored identities

### Requirement: Separated rendering surfaces
Editor SHALL remove the Window Render diagnostics window and the synthetic model Details rendering readback section. Edit Render settings SHALL open a dedicated configuration window with existing live/restart semantics. Exposure and a named visualizer selector SHALL appear in the viewport toolbar; supported modes SHALL be Lit and the six existing GBuffer modes, with unavailable pipeline modes clearly indicated. The selector and status label SHALL resolve stable visualizer identities through the shared raster option contract rather than use the wire value as an array index. Display ordering SHALL NOT alter shader meaning.

#### Scenario: Inspect a model and configure rendering
- **WHEN** a model is selected and the user opens Edit Render settings
- **THEN** Details contains authored components without a synthetic rendering diagnostics component, and the dedicated settings window excludes live profile data, exposure, visualizer and light shadow authoring

#### Scenario: Reordered visualizer display
- **WHEN** the visualizer display order changes
- **THEN** the selected label, stored numeric value and GPU meaning remain associated with the same stable mode

#### Scenario: Forward retains a latent Deferred visualizer
- **WHEN** the pipeline switches to Forward while a Deferred visualizer is stored
- **THEN** the UI indicates its unavailability without overwriting the stored selection
- **AND** unrelated HUD edits remain available, new unsupported visualizer changes retain the shared service's controlled rejection, and switching back to Deferred restores the selection
