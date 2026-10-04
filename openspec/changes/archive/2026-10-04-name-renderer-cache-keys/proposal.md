## Why

Batch, instance-record and resource-retry caches express multi-field identities through positional tuples. Hashing, view-range lookup and retry progression interpret positions separately, making future identity changes difficult to review.

## What Changes

- Introduce private, named keys for batch items, instance records and resource retries.
- Name view-range and resource-request identity queries instead of accessing tuple positions.
- Preserve key fields, exact ordering, hash mixing, retry progression, cache reuse and ownership.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `render-batching`: Cache identities and view-range ordering are explicit and stable.
- `shared-render-resources`: Retry keys separate request identity from retry generation.

## Impact

Renderer private cache definitions and their direct consumers. No public API, persistent data, cache algorithm, resource lifetime or performance feature changes. Unrelated tuple keys and MaterialConstantCache decomposition are outside this change.
