## ADDED Requirements

### Requirement: Scoped window group membership
Platform SHALL support one main window and multiple directly owned auxiliary windows through move-only scoped registrations on the owner thread. Duplicate, cross-group and invalid-owner registrations and registered ownership mutations MUST be rejected. Expired registrations MUST be safe to destroy.

#### Scenario: Add and remove members while modal
- **WHEN** an auxiliary window registers during active modality or its registration ends
- **THEN** the new member is blocked immediately or only the removed member is restored
- **AND** all other members keep their blocking state

#### Scenario: Invalid registration or failed entry
- **WHEN** registration validation fails or native modal entry fails
- **THEN** existing members retain their prior state and any partially applied entry is rolled back

### Requirement: Group-local main modal priority
During modality the main window SHALL remain above registered auxiliaries and retain native input, while auxiliaries SHALL reject native input and redirect Raise requests to Main. The main owner and SDL ownership tree MUST remain stable and no window SHALL become globally topmost.

#### Scenario: Two auxiliary windows overlap Main
- **WHEN** Main enters a protected modal flow
- **THEN** both auxiliaries become disabled and remain behind Main, including after attempted raises
- **AND** unrelated applications retain ordinary foreground behavior

### Requirement: Native state preservation and continuous synchronization
The group SHALL synchronize visibility and stacking while modal, preserve originally hidden/minimized/maximized/disabled auxiliary state, and restore ownership and enabled state on exit or cleanup. Main minimization SHALL hide auxiliaries; ending modality while Main remains minimized MUST NOT prematurely reveal them. Restoration can be deferred until Main restores.

#### Scenario: Minimize and restore Main during or after modal
- **WHEN** Main minimizes while modal and restores, including after cancelling modality while minimized
- **THEN** auxiliary visibility follows Main while original placement and non-visible state remain preserved

#### Scenario: Shutdown or repeated transitions
- **WHEN** modal state repeatedly toggles, windows are destroyed or the group is released
- **THEN** native state is restored without stale membership, ownership cycles, disabling unrelated members or destroying Main

#### Scenario: Release membership or group while Main remains minimized
- **WHEN** a live auxiliary unregisters or its group is destroyed while Main is minimized
- **THEN** its owner and enabled state are restored without showing it early
- **AND** Platform restores its original visibility when Main restores, cancels that pending work if either window dies, and transfers it if the auxiliary enters another modal registration

#### Scenario: Reparent an unregistered auxiliary with pending visibility restoration
- **WHEN** an auxiliary unregisters while Main is minimized and successfully changes or removes its owner
- **THEN** its pending visibility follows the new owner or restores on the next poll if unowned
- **AND** the previous Main no longer controls that pending work
