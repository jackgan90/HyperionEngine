## 1. Preserve format and discovery baselines

- [x] 1.1 Extend existing archive/native/registry fixtures with independent fixed-prefix and encoding expectations, current metadata header equivalence and the supported full-load versus discovery version matrix.
- [x] 1.2 Add recording range-provider tests for large bulk, whole-read rejection, corrupt bulk and bulk-only truncation; assert every requested range stays within required metadata and full loading still rejects corrupted/truncated assets.
- [x] 1.3 Add malformed native/archive prefix, short-read provider, directory/metadata truncation, extreme lengths/counts, reserved/version and budget tests, including rejection before excessive range reads or allocation.

## 2. Introduce format-owned probes

- [x] 2.1 Add NativeAsset prefix extent and bounded payload-range probe; share native header definitions/parsing with current full decoding and encoding while preserving full actual-length/digest checks and legacy dispatch.
- [x] 2.2 Add Serialization prefix extent and bounded metadata-range probe; share archive prefix/layout interpretation with the full and metadata decoders and format definitions with the encoder, retaining all directory, index, alignment and allocation checks.
- [x] 2.3 Cover exact byte-budget boundaries, short spans, maximal integer claims, current directory corruption and full legacy decode; verify fixed encoded bytes and canonical hashes are unchanged.

## 3. Route discovery through the owners

- [x] 3.1 Replace AssetRegistry's binary offset/version/directory interpretation with owner-supplied extents and probes; preserve the 32 MiB complete metadata-range budget, exact returned-range checks, header validation, per-file errors and index behavior.
- [x] 3.2 Re-run range-observation and malformed-discovery tests against the integrated path, including unsupported legacy discovery, short provider returns, no bulk reads, duplicate IDs and moved-file resolution.
- [x] 3.3 Document the staged probe/decode contract, range/allocation budgets and metadata-discovery versus full-integrity acceptance differences in NativeAssets documentation.

## 4. Verify the final implementation

- [x] 4.1 Rebuild the affected existing test targets in Debug and Release and run reflected_archives, native_asset_management and native_asset_registry against those rebuilt binaries.
- [x] 4.2 Run relevant native/shared publication, Editor content and automation asset consumer regressions; record exact commands, results and any environmental limitations.
- [x] 4.3 Run applicable formatting, path/naming, dependency-boundary and strict OpenSpec validation on the final files; obtain independent review and resolve confirmed findings without expanding format or discovery scope.
