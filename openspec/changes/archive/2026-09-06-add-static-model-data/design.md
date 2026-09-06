## Context

A single local-space primitive cannot represent a glTF model. Static scene data must be owned independently of renderer and vendor types.

## Goals / Non-Goals

Goals: Add Scene runtime module with reflected model, geometry, material, texture and node data. Define stable references, roots, transforms, bounds and validation without GPU handles. Extend Math with quaternion/TRS, camera, inverse/normal transforms and vector operations.

Non-goals: no implementation of future FBX/OBJ/COLLADA codecs, animation, compressed glTF extensions, full asset cooking, ECS or additional graphics backends in this series.

## Decisions

### 1. Decision

Scene owns FModelAsset and model node hierarchy; geometry/material/image data are CPU records without RHI references. Assets remains generic and does not depend on Scene. AssetImport will depend on both to avoid a dependency cycle.

### 2. Decision

Geometry contains position/normal/tangent/color/UV arrays and uint32 triangle indices; primitives point to material slots, nodes point to geometry and children, model roots select one scene. Preserve mesh sharing instead of duplicating transformed vertices.

### 3. Decision

Materials express glTF metallic-roughness factors, five texture slots with UV set selection, normal/occlusion scales, alpha mode/cutoff and double-sided state. Images store decoded RGBA bytes independent of usage color space; sampler and material roles decide sRGB versus linear sampling.

### 4. Decision

Use column-major, column-vector matrices and right-handed Y-up source data. Explicit camera/projection helpers use zero-to-one clip depth. World bounds and normal transforms account for nested and nonuniform transforms. Register every persistent model field once.

## Risks / Trade-offs

Negative and nonuniform scales affect normals and winding; validate with asymmetric fixtures. Reject invalid references/cycles and non-finite attributes rather than trusting importer output. No ECS, animation system or empty placeholder module.

## Migration Plan

Implement after `add-reflected-object-archives`. Keep existing target names, serialized keys, triangle and configuration tests working. Each change is additive until its replacement paths are verified; retain explicit compatibility wrappers where required. Validate focused tests before continuing and run full Debug/Release verification at the end.

## Open Questions

No blocking product decisions. Implementation refinements must be reflected here and validated before task completion.
