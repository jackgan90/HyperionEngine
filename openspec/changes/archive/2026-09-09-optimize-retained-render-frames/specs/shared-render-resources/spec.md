## ADDED Requirements

### Requirement: Ready view preparation does not schedule maintenance
Resource maintenance SHALL be requested by production, publication, owner release or unresolved completion/retirement work. Preparing a view containing only already-ready retained resources SHALL NOT by itself create an asynchronous maintenance task. Resource failures and no-frame retirement SHALL continue to progress.

#### Scenario: Repeated ready frames
- **WHEN** an unchanged ready scene renders repeated views
- **THEN** view preparation alone schedules no worker delay or RHI cache scan

#### Scenario: Last user releases without rendering
- **WHEN** the last scene/frame owner releases resources and no new frame is rendered
- **THEN** necessary completion polling and final RHI retirement still complete through the resource coordinator
