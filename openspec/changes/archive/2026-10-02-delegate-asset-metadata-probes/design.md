## Context

`AssetRegistry::ReadAssetHeader` currently reads a native header and an archive prefix, interprets their magic/version/length fields itself, calculates a directory-plus-metadata extent, then calls `DecodeArchiveMetadata`. The duplicated layout consists of the 80-byte HAST v1 envelope and HYPA v2's 24-byte prefix and 16-byte block entries. NativeAsset and Serialization already encode and decode those formats.

Discovery intentionally performs less validation than full loading. It reads current native metadata without bulk, preserves a 32 MiB archive prefix/directory/metadata budget, and can discover metadata from a file whose bulk is corrupt. Full loading checks actual file size, the payload digest, object revision and dependency consistency, and accepts legacy archive inputs that discovery does not accept.

## Goals / Non-Goals

**Goals:**

- Give NativeAsset and Serialization sole ownership of their respective prefix, version, range and directory interpretation.
- Preserve byte-for-byte encoding, format support, range reads and existing full-load validation.
- Reject malformed prefixes and excessive ranges before dependent reads or allocations.
- Make lightweight discovery guarantees and their limits explicit and testable.

**Non-Goals:**

- New disk versions, asset migrations, public automation operations, plugin behavior or filesystem APIs.
- Full digest, bulk-content, dependency or header/body consistency validation during discovery.
- A generic streaming serialization framework or reader callback that couples Serialization to filesystem services.

## Decisions

### 1. Expose two small byte-span probes

NativeAsset provides its required prefix extent and a probe returning the validated declared payload range. The probe checks input length before reading any field, current HAST magic/version, and that header plus declared payload fits the native file limit. It does not claim that the complete declared payload was supplied or exists on disk. EncodeAsset, the current-container branch of DecodeAsset and the probe share native format definitions and prefix interpretation; full DecodeAsset separately compares declared payload length with actual input length and checks integrity.

Serialization provides its required current archive prefix extent and a metadata-range probe. Given a supplied prefix, declared archive total and archive limits, it returns the extent containing prefix, directory and metadata. It validates current magic/version/reserved bits, node/count limits and all arithmetic before returning a range. The caller applies its narrower discovery policy before reading that range. A returned range means the prefix is structurally valid; directory and metadata validation finishes when the returned bytes are decoded.

Probe outputs are values containing validated sizes/ranges; they do not borrow input memory. Fixed-format sizes remain fixed owner definitions, not configurable settings. Registry can query prefix extents without knowing field offsets or directory entry size.

Alternative considered: move 80/24/16 to a shared header while retaining registry parsing. Rejected because it leaves duplicated format interpretation. An IO-aware Serialization reader is unnecessary and would reverse the memory-only boundary.

### 2. Share decoding rules inside each format owner

Serialization factors prefix/layout reading so metadata probing, metadata decoding and ordinary decoding share magic, version and size interpretation. The existing directory validation remains authoritative for contiguous block ranges, declared total, bulk indices, element alignment and trailing data. Encoding uses the same owner-local format definitions. Neither decoder bypasses its allocation, node or depth accounting through the new helper.

Full archive decoding retains its legacy branch. A current-only metadata probe must not be placed unconditionally ahead of every full read. Existing `DecodeArchiveMetadata` remains byte-span based and substitutes null bulk values without copying or hashing their content.

### 3. Keep registry as the range-read coordinator

The registry reads the native prefix, probes the native payload, reads the archive prefix at the returned offset, probes its metadata extent, enforces its 32 MiB policy, then reads and decodes that extent. A prefix read may be combined with another only through owner-supplied extents; there is no requirement to preserve exactly two read calls. Every returned range is checked before accessing it, including a provider that returns fewer bytes than requested.

The sum of archive prefix, directory and metadata must fit the discovery budget; counting only the metadata field would weaken the existing limit. The native declared payload must fit `MaxBytes - native header bytes`, with checked subtraction before addition/conversion. Full DecodeAsset continues to validate the entire supplied file against InLimits.MaxBytes before archive decoding. Existing default node/depth/allocated-byte budgets remain active during metadata decoding.

After decoding, the registry retains reflected header extraction, `ValidateAssetHeader`, per-file diagnostic handling, duplicate-ID rejection, sorting and index construction. Native envelope/object validation is not enlarged as part of this refactor.

Alternative considered: stat the whole file or read its final byte to validate declared total before discovery. Rejected because the current contract validates requested metadata ranges and declared directory structure while leaving actual bulk integrity to full loading.

### 4. Preserve the acceptance matrix

| Input | Discovery | Full load |
|---|---|---|
| HAST v1 with valid HYPA v2 payload | Accept after metadata validation | Accept after full validation |
| Bare HYPA v1 or v2 | Reject | Preserve existing legacy support |
| HAST v1 with an archive accepted by the existing legacy full reader | Discovery remains current HYPA v2 only | Preserve existing full-read support |
| Unsupported native/archive version | Reject | Preserve existing version rejection |
| Truncated native prefix, archive prefix, directory or metadata | Reject safely | Reject |
| Valid metadata/directory with corrupt bulk or only bulk truncated | Metadata can still be discovered | Reject full size or integrity check |

Probes do not validate schema migrations or asset dependency graphs. Discovery success is not a promise that the asset can load. Local and mounted providers retain actual range reads; the generic IFileSystem fallback remains outside this change.

### 5. Extend existing test executables

Use `archive_tests`, `native_asset_tests` and `asset_registry_tests`; no new target is expected. Fixed bytes and independent mutations protect the wire contract separately from encoder-generated round trips. Add coverage for each short prefix length, malformed magic/version/reserved data, extreme metadata/count values, directory truncation and overlap/gap/out-of-range totals, metadata structure budgets, and a short-read provider.

A recording range provider rejects whole-file Read, tracks requested offsets/sizes and verifies every request ends at or before the metadata boundary. It verifies excessive prefix claims fail before a large range read. Retain large-bulk header equivalence, corrupt-bulk discovery, duplicate identity and relocation tests, and add bulk-only truncation plus legacy acceptance-difference checks.

Implementation verification rebuilds affected targets for Debug and Release, runs reflected_archives/native_asset_management/native_asset_registry, then relevant publication/content consumer regressions. Existing binary baseline results do not replace those rebuilds. Style, naming when applicable, dependency boundaries and strict OpenSpec checks run on final modified files.

## Risks / Trade-offs

- Shared parsing accidentally restricts full legacy reads → keep the version matrix explicit and cover full legacy fixtures independently of discovery.
- Native and archive byte limits are mixed or the header is counted twice → test exact limits and distinguish supplied whole-file size from declared payload size.
- Large integers overflow or reserve before rejection → check span length and bounds before field access, subtraction, multiplication, narrowing or allocation; exercise maximal values.
- Prefix success is mistaken for complete directory validation → document the staged probe/decode contract and retain the common authoritative directory validation.
- An extra small range read adds IO overhead → accept the bounded call increase while proving no bulk read; optimize only within owner-provided ranges if needed.
- Metadata discovery remains possible after unseen bulk corruption/truncation → preserve this intentional behavior and document that full loading remains authoritative.

## Migration Plan

Add regression fixtures first, introduce format-owned probes and shared helpers, switch registry consumers, then run rebuilt verification. No data migration or reimport is required. Reverting this code change leaves stored bytes compatible.

## Open Questions

None blocking. Final helper/type names are implementation details; the ownership, budgets and acceptance matrix above are fixed for this change.
