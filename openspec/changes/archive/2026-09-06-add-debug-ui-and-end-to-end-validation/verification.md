# Verification — 2026-09-06

- Debug build + CTest: 13/13 passed (`out/change09-debug.log`).
- Release build + CTest: 13/13 passed (`out/change09-release.log`).
- Actual hardware: NVIDIA GeForce RTX 5080. D3D12 debug layer enabled. GPU error/corruption message count: 0.
- GUI input test toggled a checkbox and edited a reflected property through normalized mouse events; saved settings reloaded identically. Draw-data ownership, framebuffer scale, index bounds and hooked allocation cleanup passed.
- Process acceptance test exercised resize/minimize/restore, saved 960x540 configuration with scale 0.63, restarted from that configuration and captured triangle/UI pixels. It checked distinct Main/Render/RHI thread identities, nonzero Render/RHI 0/RHI 1/Worker work, single-RHI clear-only mode and rejection of an unknown plugin. Per-run evidence is under `out/build/{debug,release}/viewer-acceptance`.
- Triangle and GUI shaders compile to DXIL, SPIR-V and MSL. Texture/sampler bindings occupy distinct Vulkan bindings. Both GUI SPIR-V stages additionally passed the local `spirv-val --target-env vulkan1.1` validator. No Vulkan or Metal runtime claim is made.
- Visible Release run: 180 frames, final triangle/UI screenshot and zero GPU validation errors (`out/visible-launch.log`, `out/captures/final.png`). Final screenshot visually inspected, with readable controls and memory/thread metrics.
- Interactive Release Viewer subsequently launched and left running. Process 7068 reported window title `Hyperion | Rendering Lab`, a nonzero native window handle and `Responding=True`. Its output is in `out/interactive-stdout.log`; stderr was empty.
- All C++ sources pass clang-format verification; dependency include boundaries pass. OpenSpec strict validation passed before archival.

Known scope: Windows/MSVC x64; static logical plugins; a single imported color attachment graph; one graphics queue; synchronized CPU frame stages; explicit allocation hooks, not process-wide interception; static GUI font atlas. See `docs/architecture.md` and README.
