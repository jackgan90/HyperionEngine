## Why

Native asset discovery independently interprets container and archive prefix layouts already owned by NativeAsset and Serialization. A format change can therefore leave metadata discovery inconsistent with normal decoding even when both paths compile.

## What Changes

- Add bounded byte-span probes owned by NativeAsset and Serialization that describe the payload and metadata ranges needed by discovery.
- Share each format's structural and version interpretation with its normal encoder/decoder, while retaining separate metadata-only and full-integrity validation responsibilities.
- Make the registry orchestrate range reads through those probes, preserving current format support, the 32 MiB discovery metadata budget and local/mounted bulk-free scanning.
- Add malformed-prefix, directory, budget and compatibility coverage, including metadata discovery after bulk corruption and the existing legacy full-load support.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `native-asset-registry`: Require format-owned range discovery, bounded reads and the explicit distinction between metadata discovery and full asset validation.
- `reflected-object-archives`: Require an in-memory metadata-range probe sharing current archive layout validation with decoding and preserving legacy full reads.

## Impact

Affected code is limited to NativeAsset and AssetRegistry in Runtime/Assets, the archive APIs and implementation in Runtime/Serialization, existing asset/archive tests and native asset documentation. No disk format, stable asset/type identity, automation operation, plugin lifecycle, filesystem abstraction or dependency direction changes are intended.
