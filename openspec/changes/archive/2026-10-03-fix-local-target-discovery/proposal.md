## Why

Local target lookup currently searches a discovery list bounded to 128 candidates and 1024 directory entries. Valid stale records can hide a newly launched Editor even when its exact instance ID is known, breaking CLI/MCP attachment and repeated acceptance runs.

## What Changes

- Resolve a known instance independently of enumeration budgets, retaining transport admission, identity-checked handshakes and no fallback.
- Add versioned private local-registration metadata containing process identity and creation identity; conservatively filter and reclaim only registrations proven stale.
- Preserve reading of legacy target records without guessing that unknown ownership, access failures or handshake timeouts mean process death.
- Expose typed enumeration completion/limit information through the shared target-list response while preserving existing target descriptors and operation IDs.
- Give acceptance processes isolated per-run discovery environments and cover saturation, stale ownership, false-death avoidance and real CLI/MCP attachment.
- Document that older frontends cannot read the new private registration format and must be upgraded with the application; the new reader supports existing unversioned records.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `automation-transport`: exact instance lookup, truthful bounded enumeration and conservative local-registration ownership/retirement.
- `live-application-automation`: lifecycle-safe registration maintenance and isolated real-process acceptance coverage.

## Impact

Runtime/Automation discovery and connection resolution, a private Windows registration adapter, automation test fixtures and test launch configuration, and connection documentation. No new transport protocol, domain-operation IDs, GUI mutation path, public process requirements for direct-address connections, or dependencies on Renderer/native graphics backends. The change remains active and uncommitted after implementation and validation.
