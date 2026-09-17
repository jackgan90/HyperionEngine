## Context

Baseline engine revision is eb40123683ef18ef8e3e3bed4cfa3ff63292a1d4 and content revision is 05f5382c2cc59d0fc90728457727e4fbce331e23. Scene owns validated Main-thread nodes and transactional hierarchy mutations; Renderer publishes value snapshots through generation-safe mailboxes. Editor is read-only and navigation currently edits the scene camera. Native Sponza scene schema 5 contains one model node; its shared model already contains source hierarchy and geometry sections.

## Goals / Non-Goals

**Goals:** Compose registered CPU components, expose their properties without per-component panel code, preserve ownership and transaction guarantees, edit whole-model instances without copying their internal topology, save/reload/undo edits, migrate shipped content and quantify performance changes.

**Non-Goals:** ECS, scripting/GC, automatic reimport conflict resolution, general prefab authoring, geometry editing, dynamic DLL reload, new rendering features and git commits.

## Decisions

### Main authority and registered value components

Evolve FSceneNode into an identified object owning registered component values. Each component has an instance ID, type ID and reflected state. Built-in typed accessors adapt existing clients to this authoritative storage; they do not maintain a second mutable copy. Transform is mandatory and unique and owns Local and Parent. Name/enabled/ID remain object metadata. Built-in rendering capabilities are unique per type in this iteration, while independent types compose. Registered custom types have explicit multiplicity and dependency constraints. Components contain CPU values and focused validation/behavior; there is no mandatory Tick.

Retain FScene's transaction and generation-safe object handle contracts. Component edits operate on copies and validate the complete candidate before commit. Property targets include object handle, component ID and expected document/scene revision. Existing kind queries become capability queries; rendering metadata examines every present capability.

### Record reflection and generic inspection

Extend FRecordMember with optional typed inspection metadata and generic scalar/container/record access. Persisted field IDs remain stable. Visibility, editability and persistence are independent. Declare labels, groups, ranges, enum choices and asset/reference semantics beside the owning reflected data. GUI draws generic widgets against candidate values and returns edits; it never invokes arbitrary setters on live scene memory. Existing configuration reflection remains compatible while new component panels use the record model. Type-specific drawers are reusable and not per-component panel files.

### Transform display follow-up

Record descriptors may opt into a CPU-only display layout: project source values into another reflected record, edit an isolated draft, validate it and map changed display values back to a detached source candidate. The original display snapshot is supplied to the mapper so unchanged fields need not be reconstructed. Source validation and normal scene transactions still apply. Layouts live with their owning component definition; Gui does not recognize component types.

Transform projects its affine matrix into local Position, Rotation in degrees and Scale. Column-vector rotation is right-handed `Rz * Ry * Rx` (fixed-axis X, then Y, then Z). A complete orthonormal basis is recovered even for singular matrices; upper-triangular stretch/shear is retained while editing. Reflections use a deterministic signed-X convention where full rank permits it; equivalent Euler/scale representations are not unique. Parent-only and translation-only edits preserve the original linear matrix exactly. Opening or reverting never reconstructs a source matrix. Shear coefficients are not exposed as normal TRS fields, but remain preserved. Vector rows use labeled/color-coded X/Y/Z inputs and report per-axis bounds for GUI acceptance. Persistence schemas, source pivots and Sponza assets do not change.

### Rendering ownership

Renderer bridges component state into existing primitive/resource infrastructure. Main UI never accesses IRenderPrimitive. Render publishes copied immutable diagnostics with scene/object/component provenance, publication revision and frame/view information where needed. Stale handles, replaced scenes and stale replies cannot affect a newer selection. Diagnostic snapshots are nonpersistent; persistent settings remain Main authority. Debug commands target an owning thread and report completion separately.

### Model instances and shared geometry

Add stable source-node and primitive IDs to native model records. The default authoring unit is one scene object referencing an entire model. Its Transform owns instance placement; internal node transforms and primitive topology remain model-owned. Render derives each primitive world transform from instance world times model-internal world. Model-level and section material overrides remain instance-owned values and do not introduce scene nodes or modify shared assets.

Importing models with --scene, importing scene sources, upgrading native records and loading scenes never automatically expand model hierarchy. A future independent-parts workflow must be an explicit user action; no new expansion button is shipped in this follow-up. Keep the explicit CPU expansion helper and selected-geometry compatibility for already authored documents. Native upgrades preserve those documents, including child edits; they never silently collapse them. Scene importers advance to revision 6 and scene publication records the whole-model policy so old model-to-scene caches also rebuild. Model schema 3 and scene schema 6 remain unchanged.

Sponza migration is explicit and guarded: verify the shipped expanded subtree against its source model and reproducible recipe before replacing it with one whole-model component. Preserve root identity, placement, camera/lights and overrides. Refuse to discard child edits that cannot be represented by the compact instance. Keep shared dependency files and immutable revisions, republish the scene and catalog, and verify appearance. Reading a document never migrates its topology.

### Document editing and camera

Editor owns document history/save points, selection and pending save operations. Generic property changes and structural operations use the same transactional entry point; drag sessions coalesce, undo/redo reapply validated state and stale asynchronous work is invalidated. Save freezes authored values, uses existing native atomic IO and only advances the saved point for that captured state. Scene and shared asset edits are explicit distinct scopes. Unsupported opaque component data is preserved losslessly or save is rejected.

Editor has an independent viewport camera with the existing reusable navigation behavior. Rendering accepts an explicit frozen camera pose/lens override; scene camera selection continues to work for Viewer. Normal navigation does not dirty the document.

### Performance evidence

Freeze baseline executables and scene roots before semantic changes. Add reusable opt-in Editor benchmark reporting if required and use the exact same measurement code on baseline and candidate. Run Debug/Release static and deterministic moving camera workloads sequentially with equal size, render settings, validation, warmup and sample counts. Record ready/nonempty work, actual draws/items, mean/P95/P99, rendering CPU/GPU diagnostics where available, UI/scene/frame time, load/save latency and memory. Run multiple repetitions; retain regressions, raw samples, identities and uncertainty. Distinguish inclusive CPU work, wall time and GPU timestamps. No unready frames qualify as samples.

## Risks / Trade-offs

- Primitive count is not authoring-object count → preserve one whole-model instance by default; retain historical expansion measurements separately from the final compact candidate.
- Affine matrices can contain shear → keep affine authority and preserve the residual stretch/shear in the TRS display mapping; do not silently replace a general affine matrix with pure TRS.
- Async asset replacement/history can deliver stale results → retain epochs, handle generations and expected revisions, test delete/replace/reload races.
- Schema changes affect external content → explicit staged migration and dependency validation, preserve IDs and old immutable dependency generations.
- Generic reflection must not bypass invariants → validate candidate component and complete scene before authoritative writes.

## Migration Plan

1. Capture baseline builds, root assets, configuration and repeated measurements.
2. Implement new records and compatibility readers, registered components and transactional Inspector.
3. Keep whole-model references through import/publication and update source import/rebuild recipes; expansion remains explicit only.
4. Stage migration output, verify round-trip and matched camera rendering, then publish Sponza and catalog in HyperionAssets. Loading alone never rewrites a file.
5. Run targeted/full required tests, style/naming/boundary checks and candidate performance matrix; document measured effects and remaining explicit limits.

## Open Questions

No user decision blocks implementation. Concrete storage/helper names and UI organization may be refined while preserving these contracts.
