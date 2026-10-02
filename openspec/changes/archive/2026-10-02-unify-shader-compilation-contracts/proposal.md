## Why

Shader register shifts are encoded in the compiler and independently decoded by reflection. Actual profile, HLSL version and SPIR-V target policy also live outside the cache identity's explicit inputs. M06 and M10 share the Shaders ownership boundary and can remove these maintenance hazards together while retaining separate acceptance evidence.

## What Changes

- Define one typed b/t/s/u register protocol with encoding, decoding and checked space/register/array bounds; use it in compiler arguments and reflection.
- Describe the actual compilation target in one private value used to construct DXC arguments, diagnostics and cache identity.
- Retain existing resource kinds, register limits/mapping, source snapshots, virtual includes, toolchain/reflection identities and explicit versioning for non-described pipeline changes.
- Add independent fixed mapping samples, rejection boundaries and policy-sensitive cache tests alongside real cold/hot DXIL/SPIR-V/MSL compilation.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `shader-pipeline`: Require a shared register protocol and cache identity derived from the actual applicable compilation target policy.

## Impact

Runtime/Shaders public register definitions and private compiler/reflection adapters, shader tests and Materials documentation. No new compiler target/profile support, public user configuration, backend, disk asset format, automation operation, plugin lifecycle or dependency direction change. M06 mapping and M10 cache identity remain separately verifiable checkpoints.
