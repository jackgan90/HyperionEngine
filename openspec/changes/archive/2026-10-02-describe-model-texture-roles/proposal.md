## Why

Model material conversion maintains texture bindings, texture/sampler/UV semantics and color encodings through parallel arrays and positional checks. Sampler conversion also infers behavior from enum ordering, making otherwise local changes require manual synchronization.

## What Changes

- Describe each supported model texture role in one AssetImport-owned private row containing its source member, target semantics, encoding and stable output order.
- Convert bindings from those rows while preserving material value order, named products, shared texture identities, fallback resources and content revisions.
- Translate source sampler enums through explicit rules without changing serialized enum values or defaults.
- Add independent role/sampler expectations, historical output fingerprints and descriptor-order regression coverage.

## Capabilities

### New Capabilities

- `model-material-conversion`: Explicit source-role mapping and order-independent, compatible native material conversion.

### Modified Capabilities

None.

## Impact

AssetImport model conversion, module-private conversion tests, asset import test registration and asset pipeline documentation. Materials retains ownership of existing semantics. Public import APIs, source formats, operation IDs, wire schemas, importer versions, plugin dependencies and publication/lifecycle behavior remain compatible; no Renderer or RHI changes are required.
