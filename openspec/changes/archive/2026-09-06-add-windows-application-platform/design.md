## Context
Core, task, configuration and plugin foundations are implemented. Platform window operations must remain on Main.
## Goals / Non-Goals
**Goals:** SDL3 window, normalized input, native surface token for the RHI adapter, resize/minimize/restore/close, CLI-driven bounded startup.
**Non-Goals:** GPU rendering or multiple simultaneous windows.
## Decisions
- Keep SDL_Window and SDL_Event private. Translate input into engine events usable by the future GUI adapter.
- Expose an opaque native surface token for backend integration. Windows-specific token interpretation remains inside the future DX12 adapter.
- Own initialization, polling and destruction on the creating Main thread. Use RAII for SDL cleanup.
- Viewer supports --config, --frames, --hidden and --exercise-window for deterministic lifecycle testing.
## Risks / Trade-offs
- Desktop permissions vary in automated sessions → preserve application logs and test actual visible startup separately from hidden CTest runs.
- Minimized windows can have no drawable area → expose minimized state and pixel dimensions for future render throttling.
