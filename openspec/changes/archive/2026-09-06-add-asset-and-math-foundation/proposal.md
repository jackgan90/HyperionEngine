## Why
Rendering experiments need stable math, mesh and image types without coupling algorithm code to codecs or math vendors.
## What Changes
- Add engine math types backed by GLM and minimal static glTF mesh loading through cgltf.
- Add PNG and floating point EXR image I/O through stb and TinyEXR.
- Persist typed asset references through the existing reflection system.
## Capabilities
### New Capabilities
- `asset-math-foundation`: Vendor-independent math, mesh and image I/O and reflected asset references.
### Modified Capabilities
None.
## Impact
New assets library, codec adapters and fixture tests; private GLM, cgltf, stb and TinyEXR dependencies.
