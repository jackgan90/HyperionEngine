## ADDED Requirements

### Requirement: Startup-resolved independent storage
Applications SHALL resolve typed user-data, cache and bootstrap-settings paths before initializing writable runtime services. Stable application/profile identities SHALL scope user files independently of source/build/executable location. Explicit root and locator overrides SHALL permit operation without writing to the platform default directory. Content mounts SHALL remain separate.

#### Scenario: Off-system-drive startup
- **WHEN** the user selects writable locator, user-data and cache paths on another drive
- **THEN** startup uses those paths without requiring writes to the default user directory or repository out

#### Scenario: Relocated build
- **WHEN** an executable with the same application/profile identity is rebuilt at another build path
- **THEN** default user settings and caches remain at their independently resolved paths

### Requirement: Revisioned restart-only storage settings
One UI-independent service SHALL validate, persist and report root changes. Active roots SHALL remain frozen until restart. Reflected get/set operations SHALL report saved roots, effective roots, revision, override provenance and restart state. Stale revisions and persistence failures SHALL preserve the previously published settings.

#### Scenario: Saved relocation
- **WHEN** GUI or automation submits valid new roots with the current revision
- **THEN** the same domain operation saves them and reports restart state without changing open logs, content handles or active cache roots

#### Scenario: Conflicting or unwritable edit
- **WHEN** a candidate overlaps protected persistent storage, is unwritable, or uses a stale revision
- **THEN** the edit fails with a diagnostic and previous saved and active state remain intact

#### Scenario: Pinned launch override
- **WHEN** a launch override masks a saved root
- **THEN** queries and Editor disclose the override and do not claim the saved value will supersede it

### Requirement: Non-destructive configuration migration
Migration SHALL import only missing known legacy Editor configuration/state files, retain originals, and prevent repeated legacy resurrection. Restart relocation SHALL transfer configuration/state without bulk copying logs, captures, caches or arbitrary evidence. Isolated runs SHALL NOT import interactive legacy preferences.

#### Scenario: Existing destination
- **WHEN** migration finds an existing destination setting
- **THEN** that destination remains authoritative and the original is retained

#### Scenario: Next launch after relocation
- **WHEN** a saved root change is applied on a later launch
- **THEN** configuration/state saved by the previous session is available at the new root and unrelated old out files remain untouched

### Requirement: Development and deployment compatibility
Applications SHALL share storage rules in development and deployment. Installed Content and shipped defaults SHALL be readable independently of source-checkout availability; runtime writes SHALL use user storage. Required published assets SHALL NOT exist only in disposable caches.

#### Scenario: Read-only moved installation
- **WHEN** a deployment is moved and its installation directory is read-only
- **THEN** it resolves shipped content there and writes configuration, logs and cache only to the selected writable roots
