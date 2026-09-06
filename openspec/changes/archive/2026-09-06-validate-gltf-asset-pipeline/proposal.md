## Why

Successful parsing and a triangle screenshot do not demonstrate full asynchronous model loading. Regressions must cover IO control, supported glTF semantics and visible rendering.

## What Changes

- Add reproducible format and lifecycle fixtures plus automated model screenshot/readback acceptance.
- Cover delayed IO responsiveness, one-worker progress, duplicate loads, malformed input, cancellation and shutdown.
- Document supported static glTF features, limitations, module boundaries and reproducible usage.
- Run style, naming, Debug/Release builds and complete regression suites; retain verification evidence per change.

## Capabilities

### New Capabilities
- `gltf-pipeline-validation`: Successful parsing and a triangle screenshot do not demonstrate full asynchronous model loading. Regressions must cover IO control, supported glTF semantics and visible rendering.

### Modified Capabilities
None. Existing configuration, triangle and task behavior is preserved.

## Impact

Tests, fixtures, docs and OpenSpec verification. Depends on add-static-model-rendering; FBX/OBJ/COLLADA, animation, skins, morph and compressed extensions remain future work.
