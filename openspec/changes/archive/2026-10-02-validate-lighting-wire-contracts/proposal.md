## Why

Lighting structured buffers already have authoritative Materials shader contracts, but CPU records are checked mainly by total size and binding strides are repeated as literals. Field reordering can preserve that size while changing GPU meaning. Local-light attenuation and cone components are also packed separately in clustered and light-volume paths.

## What Changes

- Validate supported CPU lighting wire members, offsets, physical types and record stride against the existing structured shader contracts before upload.
- Derive binding strides from the validated contract and preserve current record bytes, semantics and versions.
- Share named CPU local-light encoding and named HLSL decoding across clustered and light-volume consumers.
- Add fixed independent word/byte expectations, same-size layout mismatch rejection, cross-target reflection and D3D12 field readback evidence.
- Preserve cluster assignment, buffer reuse, directional selection and old-frame ownership behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `engine-shader-semantics`: Validate direct CPU wire records against existing structured contracts and use the validated stride.
- `clustered-local-lighting`: Share explicit local-light encoding semantics with volume lighting and preserve immutable published source reuse.

## Impact

Focused Materials wire-validation support; Renderer clustered/directional/light-volume producers; lighting HLSL helpers; related CPU, shader and GPU tests and documentation. No new shader resource protocol, format migration, reflection registry, backend or general-purpose object serializer. Layout and encoding are separate acceptance checkpoints within one change.
