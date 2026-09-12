## Context

The current model record embeds glTF-specific materials, decoded RGBA images and samplers. Runtime preparation rebuilds texture sources and material definitions per model. Generic Materials already supports typed numeric/structure/array/resource values, shader paths and defines, render state and multiple passes; Renderer owns compilation, reflection and fenced resource retirement. Native Assets already provides typed references, pinned revisions, shared immutable loads and transactional save ordering.

## Goals / Non-Goals

**Goals:** Store independently editable, reflected material/texture assets; share them across independent models and scenes; use general material descriptions; keep source conversion offline; support old embedded model upgrade; retain render behavior and instance isolation; prove CPU/GPU reuse and authored shader execution.

**Non-Goals:** Compiled shader assets, a visual material graph editor, texture compression/virtual texturing, arbitrary runtime render-target or buffer persistence, automatic orphan garbage collection, or Git submission.

## Decisions

### Durable data and runtime ownership

A CPU-only Textures module owns validated RGBA8 texture mip records and linear/sRGB encoding. Sampler and UV choices are material data. Materials owns a durable material record corresponding to its existing pass, shader, state and parameter contracts. Persistent values carry typed numeric words, structures/arrays, samplers or typed texture references; runtime texture pointers, read buffers and render targets cannot be serialized. Semantic declarations may omit a runtime-only default and receive the existing engine semantic fallback during preparation.

Persistent model schema 2 contains geometry, hierarchy and material references. Embedded glTF data remains an explicit import/procedural source representation, separate from the native model contract. Record migration never performs IO: old embedded model input is split by AssetTool into a complete native graph. Direct runtime use of unsupported legacy embedded model records reports the upgrade requirement.

### Reflected shader and parameter contracts

Each material stores the existing pass usages, vertex/pixel paths, entries, defines, instance-array fallback metadata, render state and typed parameter declarations. Rooted source paths remain governed by the existing shader compiler. glTF import uses a CPU material preset to produce the current Forward, HDR, DeferredBase and ShadowDepth behaviors; ordinary native model loading never reconstructs shader descriptions from fixed PBR fields.

### Named import products and sharing

Import conversion can return a root and named typed products. The importer derives stable subasset keys from source identity plus material/image index and texture interpretation; external image identity uses canonical source path plus encoding/mip settings. Embedded images use their owning source plus image index. Equal numeric material values do not merge distinct editable materials.

A shared library index maps stable import keys to asset IDs and immutable revision files. Library location is explicit, defaulting to the output directory; content builds supply one common library for independent roots. Library publication is serialized by an engine IO lease. Existing pinned references remain exact. The publisher writes complete immutable dependencies and the index before replacing the requested root atomically; failed work leaves the previous root graph valid. Old generations are retained. All participating source fingerprints and importer settings remain tracked for incremental rebuild.

### Native graph resolution and reuse

Renderer resolves the model graph using registered model/material/texture records and the existing generic asset service. A shared material asset cache maps loaded immutable material identity/revision to one definition and material snapshot; texture assets map to one immutable CPU texture source. GPU texture and material caches continue using those shared source/definition identities and existing fences. Geometry resource identity remains tied to model geometry, not local material parameters. New material revisions produce new immutable state while old frames retain their required versions.

### Scene edits

Scene records store optional whole-model and per-section material references plus typed local parameter values. Existing simple PBR controls remain convenience overrides. Runtime association retains the durable reference alongside the resolved snapshot. Saving captures these references and values, rebases paths for Save As and rejects procedural resources with no persistent representation. Reload resolves the same graph and preserves per-instance isolation.

## Risks / Trade-offs

- [Schema transition] → Separate source DTO and native contract, explicit legacy upgrade and before/after rendering tests.
- [Shared cache lifetime] → Immutable loaded objects, bounded/weak CPU cache ownership and existing GPU fence retirement; validate identity and resource counters.
- [Cross-root publication concurrency] → Exclusive library lease, immutable revision files and root-last publication; failure/concurrency probes.
- [Color interpretation changes] → Include encoding/mip settings in import identity and test sRGB/linear variants without conflating sampler identity.
- [General shaders with unsupported passes] → Reuse existing compile/reflection validation and ordinary capability fallback; fail unsupported authored bindings with diagnostics.
- [Large texture graphs] → Keep existing graph/serialization budgets and perform mip generation offline.

## Migration Plan

Add record contracts and tests, then generic import products/library support and legacy split upgrade. Integrate renderer graph resolution and scene persistence, rebuild native sample content, run Debug/Release unit and GPU acceptance suites, and document measured sharing and remaining scope. Old roots remain untouched on failed import. Source changes remain uncommitted for review.

## Open Questions

None blocking implementation. Exact helper APIs and cache container choices may evolve while preserving the contracts above.
