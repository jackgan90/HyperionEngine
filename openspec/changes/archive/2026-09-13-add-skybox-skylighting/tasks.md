## 1. HDR and cube resource contracts

- [x] 1.1 Extend native textures with dimension/format/face validation and compatible schema migration.
- [x] 1.2 Extend Materials, DXIL/SPIR-V reflection and RHI typed cube bindings with mismatch validation.
- [x] 1.3 Implement floating-point six-face/mip D3D12 uploads and retain existing resource lifetime behavior.

## 2. Offline environment assets

- [x] 2.1 Add the CPU Environment module, sky record and reusable cubemap/SH/GGX/BRDF processing.
- [x] 2.2 Add tracked HDR/EXR float decode and native sky importer with bounded, versioned bake settings.
- [x] 2.3 Add numerical and asset round-trip/import validation, including HDR values and face orientation.

## 3. Scene and lighting integration

- [x] 3.1 Extend scene environment state, reflection/migration and source JSON parsing.
- [x] 3.2 Implement asynchronous complete-generation environment preparation, publication, readiness and persistence.
- [x] 3.3 Add shared scene environment semantics and Forward/Deferred/clustered diffuse and specular IBL.
- [x] 3.4 Add far-depth sky rendering after opaque compatibility and before transparency for both depth conventions.

## 4. Viewer and reference content

- [x] 4.1 Download three CC0 environments, record provenance/hashes and integrate native content generation.
- [x] 4.2 Add SceneViewer shipped/native path selection, apply/status and persistent environment controls.
- [x] 4.3 Migrate Sponza to a sky asset and calibrate documented default lighting/exposure.

## 5. Verification and delivery

- [x] 5.1 Validate real GPU direction, roughness, Forward/Deferred/clustered, depth/transparent and retained-resource behavior.
- [x] 5.2 Validate rapid replacement/failure, native-only loading and Save As/reload of sky selections.
- [x] 5.3 Verify GUI controls and all shipped environments, capturing representative images and moving/static resource metrics.
- [x] 5.4 Run required style/naming/boundary checks, Debug/Release builds and appropriate regressions; document results and limitations, keeping implementation uncommitted through the quality audit.
