## Context
The hardware RHI can clear/present and record independent contexts. Static plugin lifecycle and shader compilation exist.
## Goals / Non-Goals
Goals: a complete triangle path, correct pass ordering and graph validation, multiple CPU recording threads. Non-goals: transient textures, full resource DAG, multi-queue GPU execution, aliasing and culling.
## Decisions
- First graph explicitly targets one imported swapchain color attachment. Each pass declares Clear, Discard or Load; Load requires prior contents. Color hazards impose insertion-order dependencies, with additional explicit pass dependencies validated by topological sorting.
- Compilation emits the Present-to-RenderTarget transition at the first pass and a final presentation transition. CPU command recording can run concurrently because graph ordering is enforced during GPU submission.
- Main creates a frame snapshot; Render builds the graph and coordinates jobs; RHI 0 begins/submits the frame, and indexed RHI threads record disjoint contexts. Main waits for CPU frame completion, while GPU frames use the RHI fence ring.
- Triangle is a static RenderPlugin selected by persisted IDs. It owns immutable geometry/pipeline handles and contributes a Load pass. Shader compilation uses Worker tasks during startup.
- A clear-only mode validates plugin absence independently of triangle drawing.
## Risks / Trade-offs
- The graph is intentionally limited to a single color target → public API states this scope; general resource handles and offscreen passes require a future capability change.
- CPU frame stages are synchronized → simpler ownership first; overlap Main/Render snapshots later without changing plugin contracts.
- Application shutdown drains GPU work before stopping plugins, then destroys the device on RHI 0.
