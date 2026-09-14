# external-content-repository Specification

## Purpose
Define engine and sample content ownership, native distribution through Git LFS, recoverable source recipes and migration acceptance.
## Requirements
### Requirement: Content ownership and distribution
The engine SHALL track only engine-essential content under Content, including text shaders and the shared BRDF LUT. HyperionAssets SHALL hold non-engine native content and text shaders, track hasset files through Git LFS and retain provenance and license metadata. Raw downloaded inputs SHALL remain untracked local cache data.

#### Scenario: Native-only distribution
- **WHEN** external content is checked out with LFS objects available and mounted as Game
- **THEN** samples load without original glTF, images or HDR/EXR inputs

### Requirement: Recoverable content recipes
A versioned manifest SHALL record fixed source locations, hashes, import settings, authored recipes and stable logical source identities. Explicit tooling SHALL reconstruct inputs in an ignored cache and publish validated native graphs using AssetTool.

#### Scenario: Rebuild on another machine
- **WHEN** the manifest is rebuilt with a different source cache directory
- **THEN** asset identities remain stable and unchanged inputs do not cause unnecessary native rewrites

### Requirement: Migration acceptance
Migration SHALL verify dependency closure, relocation, source isolation, scene save/reload, text shader compilation and rendering before delivery. Both repositories SHALL remain uncommitted until explicit user authorization to commit.

#### Scenario: New structure acceptance
- **WHEN** legacy asset directories are unavailable and the new mounts are configured
- **THEN** Sponza and supplied sky/material examples operate with no dependency on the old paths
