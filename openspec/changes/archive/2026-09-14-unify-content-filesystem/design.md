## Context

The existing IO storage boundary is reusable, but native services normalize operating-system paths, shaders bypass it, and Viewer builds reimport tracked raw assets. Sample content includes original procedural fixtures and authored scene settings as well as upstream models and HDR images. Shader text remains text by explicit user clarification.

## Goals / Non-Goals

Goals: portable mounted content, strict engine/sample ownership, native distribution with text shaders, reproducible offline conversion, preserved render behavior and uncommitted acceptance in both repositories.

Non-goals: shader hasset metadata, archive containers, hot mount replacement, automatic old-generation deletion, Git history rewriting, commits or remote publication.

## Decisions

- Extend IO with package path normalization and an injected mounted IFileSystem facade. FIOService retains asynchronous IO dispatch and local/memory backends. Explicit local paths remain supported for import sources, outputs and isolated tests; runtime configured content uses virtual paths. Unknown virtual roots never fall back to disk. Mounts freeze before service use.
- Canonical package paths use UTF-8 forward slashes and exact case. Reject traversal outside roots, overlapping mounts/physical aliases, malformed names and descendant symbolic links and junctions. Local adapter operations remain the only disk boundary. Mount permissions cover writes and leases as well as reads; enumeration returns package paths.
- Normalize before native cache/graph/write ordering. Preserve ID/type/revision checks, immutable snapshots and cancellation isolation. Add catalogs by identity with conflict detection. Legacy relative references remain readable; publication and saved mounted scenes emit virtual references.
- Plain-text engine shaders live in Content/Shaders; Triangle and SharedAsset sample shaders live in HyperionAssets/Shaders. Main source/includes use the same filesystem. Cache identity contains logical paths, relevant source contents, compile options and toolchain; cache files remain local generated data.
- Publish the existing GGX Smith BRDF bake into Content/Textures with stable identity. Sky imports reference this existing Engine asset. Publisher validates external mounted native dependencies and leaves them external, avoiding accidental duplication into Game.
- External Metadata stores pinned source files/hashes/licenses, authored scene/material recipes, generator settings and stable identity metadata. Raw inputs are reconstructed into an ignored cache. Source keys use logical source-root identity and relative paths, not machine paths. Preserve migration identities and pinned references; hash changes propagate child-first.
- Mount configuration is portable JSON resolved relative to the configuration file, with a local override/CLI entry. Development defaults map Engine to Content and Game to the sibling HyperionAssets checkout. Application composition installs mounts; CPU Scene/Environment remain independent of Renderer/RHI.
- Ordinary builds consume Content directly and do not import Game. Keep existing target names; explicit tooling prepares sample content. Generated small test fixtures remain under out. External assets use LFS, shaders/metadata use ordinary Git.

## Risks / Trade-offs

- Path aliases can duplicate caches or escape mounts -> canonical validation and focused mount tests.
- Cross-repository identity changes can break pinned references -> migration dependency validation and relocation/save/reload tests.
- Old output contains unreachable generations -> export reachable runtime closure; preserve old local output until verification completes.
- Shader include behavior differs across DXC targets -> cold/warm compilation and DXIL/SPIR-V/MSL regression checks.
- Source recovery cannot reconstruct authored lights from upstream -> retain complete authored recipes in metadata.

## Migration Plan

Create metadata/cache from existing sources, capture baseline, implement IO and native integration, integrate shaders and BRDF, publish/validate external content, switch config/build/tests/docs, remove tracked old source assets only after new content is verified. Keep local recovery data. Run relocation and source-isolation acceptance plus appropriate Debug/Release checks. Leave both Git indices/commits untouched and record evidence in verification.md.

## Open Questions

None blocking. LFS remote upload and release commit pairing occur after user acceptance, outside this change's execution boundary.
