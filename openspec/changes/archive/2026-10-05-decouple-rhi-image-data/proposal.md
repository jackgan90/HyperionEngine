## Why

RHI consumes CPU image values through an Assets header that also exposes geometry, reflection and image codecs. This makes the device/upload/readback contract inherit unrelated asset service dependencies.

## What Changes

- Give image dimensions, RGBA storage and color encoding a single data-only owner with standard-library dependencies.
- Preserve Assets include compatibility and keep decoding, encoding and file operations in Assets.
- Migrate RHI and relevant public consumers to the data contract, with explicit direct dependencies.
- Protect RHI's dependency closure against Assets and compile a public RHI image consumer.

## Capabilities

### New Capabilities
- `cpu-image-data-contract`: Service-independent CPU image values and preserved image transfer semantics.

### Modified Capabilities

None. Existing screenshot, image codec and persistence behavior remains unchanged.

## Impact

Runtime/ImageData, Assets, RHI, Renderer, Gui, native backend consumers, target dependency validation, public consumer coverage and current ownership documentation. Existing CMake target names and public image type names remain stable.
