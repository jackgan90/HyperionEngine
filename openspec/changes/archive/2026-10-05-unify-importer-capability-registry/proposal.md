## Why

Workspace capabilities and validation repeat the source extensions already declared by converter registrations. The duplicated lists can disagree when an importer changes or a host selects a different importer set.

## What Changes

- Make importer descriptors authoritative for output type, extensions, conversion and workspace/tooling exposure.
- Derive workspace discovery, validation and GUI source filters from its selected, frozen importer set.
- Allow application composition to supply or disable startup descriptors through asset-service options, and apply requested texture/model names consistently across direct and draft imports.
- Preserve built-in supported formats, schemas, publication and noncancellable accepted tasks; retain explicit tooling-only conversion.
- Verify a test importer for an existing output type without adding formats or transport branches to production.

## Capabilities

### New Capabilities

- `importer-capability-registry`: Consistent descriptor-based discovery, selection and validation with startup registration and conflict checks.

### Modified Capabilities

None. Existing import protocol and default formats remain unchanged.

## Impact

Runtime AssetImport registrations/service/workspace, ApplicationServices startup options, Editor import choices, Automation capability projection and import regressions. Publication leases, output containment, history and content-root retirement retain their existing ownership.
