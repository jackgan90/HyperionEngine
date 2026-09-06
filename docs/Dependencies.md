# Locked dependencies

All direct calls are isolated behind the indicated engine wrapper. Full commits and archive digests are in `dependencies.lock.json`.

| Dependency | Pin | Wrapper / use |
|---|---|---|
| [cgltf](https://github.com/jkuhlmann/cgltf) | v1.15 | Private static-scene AssetImport adapter and legacy mesh assets |
| [d3d12ma](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator) | v3.2.0 | RHI resource allocation |
| [dxc](https://github.com/microsoft/DirectXShaderCompiler) | v1.9.2607 | Shader compilation |
| [glm](https://github.com/g-truc/glm) | 1.0.3 | Math |
| [imgui](https://github.com/ocornut/imgui) | v1.92.9b | Gui |
| [implot](https://github.com/epezent/implot) | v1.0 | Gui plots |
| [json](https://github.com/nlohmann/json) | v3.12.0 | Reflection serialization |
| [mimalloc](https://github.com/microsoft/mimalloc) | v3.5.1 | Core allocation / PMR |
| [onetbb](https://github.com/uxlfoundation/oneTBB) | v2023.1.0 | FTaskSystem workers |
| [renderdoc](https://github.com/baldurk/renderdoc) | v1.37 / cd94206b0fd9 | Optional API header only; Runtime/Capture private adapter, MIT license in header |
| [sdl](https://github.com/libsdl-org/SDL) | release-3.4.16 | Platform window / input |
| [spdlog](https://github.com/gabime/spdlog) | v1.17.0 | Core logging |
| [spirv-cross](https://github.com/KhronosGroup/SPIRV-Cross) | 83fa691cb860 | Shader reflection / MSL |
| [stb](https://github.com/nothings/stb) | 2c980bb59875 | In-memory PNG/JPEG decoding, PNG encoding and legacy image I/O |
| [tinyexr](https://github.com/syoyo/tinyexr) | v3.2.0 | EXR image I/O (bundled miniz) |
| [tracy](https://github.com/wolfpld/tracy) | v0.14.1 | Core profiling |

Dependencies remain unmodified upstream sources under `out/deps`. Miniz is the TinyEXR package dependency, not a separate engine API. Upstream licenses are included with each downloaded package. CMake checks package identity against the lock.
