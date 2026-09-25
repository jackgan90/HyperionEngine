## Why

Editor users cannot configure external asset conversion without scripts or automation, although the native publication pipeline already exists. GUI and agents need the same configurable import workflow and observable results, including independent texture import.

## What Changes

- Add File > Import Asset with a dedicated non-modal panel for source, conversion, output, advanced options and task results.
- Extract import orchestration into a shared Runtime domain service owned by a statically selected tooling plugin, independent of automation availability.
- Preserve asset.import and add discoverable validation, capabilities and application-scoped task queries.
- Add PNG/JPEG conversion to the existing texture asset type and expose existing HDR/EXR sky bake settings directly.
- Reuse current glTF/GLB, typed JSON, sky recipe and native upgrade paths, publication leases, identities and incremental checks.
- Preserve non-cancellable publication, existing open drafts, content-root lifecycle and session job isolation.

## Capabilities

### New Capabilities
- `editor-asset-import`: Shared import workflow, Editor panel, typed automation parity and lifecycle.

### Modified Capabilities
- `offline-asset-import`: Standalone texture conversion and explicit sky bake options participate in incremental provenance.

## Impact

Runtime AssetImport and Platform, ApplicationServices tooling composition, Editor GUI, Automation adapters, AssetTool option parsing, CMake, documentation and focused CPU/desktop acceptance tests. No new native asset types, transport branches, renderer changes or external dependencies. No archive or Git commit in this delivery.
