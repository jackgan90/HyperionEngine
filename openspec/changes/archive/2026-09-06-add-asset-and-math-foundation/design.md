## Context
Core allocation, reflection, execution domains and window wrappers already exist. All dependency sources are pinned and downloaded.
## Goals / Non-Goals
Goals: engine-owned column-major math, static mesh loading, RGBA image I/O and persistent asset identity. Non-goals: scene import, animation, material authoring and streaming.
## Decisions
- GLM stays private; public matrices use column-major storage and column vectors, avoiding vendor ABI exposure.
- cgltf imports one explicitly selected triangle primitive into owned CPU vertices/indices, rejecting unsupported topology. Scene transforms are outside this primitive loader.
- Images carry an explicit linear/sRGB encoding; PNG I/O preserves encoded values, EXR stores linear floats. No implicit gamma conversion.
- AssetReference uses the existing typed reflection descriptors; it persists identity and source location without serializing GPU objects.
- cgltf allocations use the Assets memory tag. Codec-owned and standard-library allocations are not claimed as fully intercepted.
## Risks / Trade-offs
- Limited glTF scope → explicit API primitive selection and unsupported-mode errors.
- Image size overflow → validate dimensions and buffer lengths before writing.
- PNG precision → documented 8-bit quantization; EXR float round-trip test.
