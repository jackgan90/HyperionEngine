## Why
Application state must be reproducible, and rendering features need a static plugin lifecycle before a window or renderer is introduced.
## What Changes
- Add engine-owned scalar/property/type metadata and validated versioned JSON persistence.
- Add application settings and explicit static plugin factories with dependency ordering and rollback.
## Capabilities
### New Capabilities
- `configuration-and-plugins`: reflected configuration persistence and static plugin lifecycle.
### Modified Capabilities
None.
## Impact
Introduces nlohmann/json only behind the serialization adapter and adds plugin/configuration targets and tests.
