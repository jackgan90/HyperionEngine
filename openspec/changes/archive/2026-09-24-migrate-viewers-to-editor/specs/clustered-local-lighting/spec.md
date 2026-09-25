## MODIFIED Requirements

### Requirement: Default enablement and measurable acceptance
Cluster lighting SHALL default enabled, expose persisted settings/CLI/GUI control and report its actual algorithm and list statistics. Disabling SHALL stop cluster construction and restore previous Deferred volumes and Forward local-light exclusion. Delivery SHALL compare the unchanged current Editor Sponza scene against the old rendering path and record real D3D12 validation, numerical image differences and stationary/moving workload measurements.

#### Scenario: Existing Sponza appearance
- **WHEN** loading the existing Sponza scene with default settings
- **THEN** its three authored point lights retain the previous appearance within documented tight numerical tolerance without retuning scene assets

#### Scenario: Toggle fallback
- **WHEN** clustering is disabled in either pipeline
- **THEN** rendering matches the previous version and no stale cluster contribution remains
