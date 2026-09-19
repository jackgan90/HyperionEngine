# plugin-application-host Specification

## Purpose
Define a minimal Main-owned application host with startup-selected plugins, scoped typed services and contributions, dependency-ordered lifecycle, and safe observable shutdown.
## Requirements
### Requirement: Main-owned application host
The application host SHALL own time, TaskSystem Main pumping and static plugin updates without requiring graphics, GUI or assets. Start, Update, Quiesce and Stop SHALL execute on the owner thread with finite nonnegative delta time and stable frame sequence.

#### Scenario: Empty host
- **WHEN** a host runs a bounded number of frames with no plugins
- **THEN** it completes without constructing a window or graphics device

#### Scenario: Non-drawable application
- **WHEN** a window is minimized
- **THEN** logical and asynchronous control progress continues while rendering is skipped

### Requirement: Typed scoped services and contributions
Plugins SHALL declare provided, required and optional service types. Required providers SHALL start before consumers. Registrations and notifications SHALL be scoped to a host and publisher lifetime. Services SHALL remain valid through consumer shutdown.

#### Scenario: Failed provider publication
- **WHEN** a provider publishes a service and then fails to start
- **THEN** its publications and subscriptions are removed, dependent consumers are skipped, and unrelated plugins remain usable in tolerant mode

#### Scenario: Scoped events
- **WHEN** a stopped plugin previously subscribed to a typed event
- **THEN** later publication does not invoke the stopped plugin

#### Scenario: Stateful and reentrant events
- **WHEN** a subscription retains mutable state and publishes another event or changes subscriptions during dispatch
- **THEN** the same callable retains its state, removed subscriptions are skipped, and new subscriptions first observe the next publication

### Requirement: Plugin-owned application features
Viewer and Editor SHALL run as application feature plugins using reusable plugin-owned asset, window, graphics and GUI services. Application entrypoints SHALL supply configuration and compiled catalogs. Native backend selection SHALL remain in application composition.

#### Scenario: Viewer and Editor startup
- **WHEN** either application starts with its normal profile
- **THEN** provider dependencies govern startup and reverse teardown while existing CLI and rendering behavior remain available

### Requirement: Drained shutdown
The host SHALL quiesce producers and drain admitted frame and tracked work before destroying consumers or service providers. Partial startup and repeated stopping SHALL safely release initialized state. Cleanup failures SHALL remain observable by the host even when the plugin factory or Start fails.

#### Scenario: Exit with asynchronous work
- **WHEN** a feature stops while frame or asset work remains admitted
- **THEN** its referenced state survives until that work has completed or been cancelled and joined

#### Scenario: Failed startup cleanup
- **WHEN** a factory or Start fails and a registered cleanup also throws during tolerant rollback
- **THEN** the original startup diagnostic remains available, remaining cleanup actions still run, unrelated plugins remain active, and the host reports the first cleanup failure at shutdown

### Requirement: Graphics-owned final validation
The graphics provider SHALL check GPU validation after dependent consumers, the render session and swapchain have been released, while its device is still alive. Device work and destruction SHALL remain on RHI 0, and any detected validation failure SHALL be reported to application control on Main after releasing the device.

#### Scenario: Late shutdown validation error
- **WHEN** an earlier application check reports no errors and swapchain teardown adds a validation error
- **THEN** the final graphics check reports failure to the host and still releases the device

#### Scenario: Clean shutdown
- **WHEN** no GPU validation error is recorded during shutdown
- **THEN** the graphics provider completes without reporting a failure, including repeated Stop calls

#### Scenario: Final GPU wait failure
- **WHEN** the graphics provider's final GPU wait throws
- **THEN** it still destroys the swapchain before the device on RHI 0, reports the wait failure on Main, and safely accepts repeated Stop calls
