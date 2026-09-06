## Context
Windows window and shader wrappers are available. Native graphics calls must remain inside the RHI adapter.
## Goals / Non-Goals
Goals: real hardware DX12 presentation, safe GPU lifetime, per-context parallel CPU recording and readback. Non-goals: Vulkan/Metal, async GPU queues, bindless, general texture formats or resource aliasing.
## Decisions
- Engine-owned resource handles use private shared state. Immutable upload buffers serve small prototype geometry; textures use default memory and a staging copy. D3D12MA owns both.
- One graphics queue and two fenced frame contexts establish predictable ownership. Each frame has up to 16 independently recorded command contexts. GPU submission is ordered on the RHI coordinator; recorded packets retain resources until their frame fence completes.
- Main owns SDL; rendering coordination and GPU submission run through dedicated execution domains. Recording different contexts is allowed concurrently; frame begin/end, resource creation and resize are serialized by the caller on RHI 0.
- Minimal pipeline descriptions cover vertex attributes, root matrix constants, optional texture/sampler and blending. No native API types escape.
- Resize drains GPU work before releasing backbuffers. Screenshot copies use row-pitch-aware readback and explicit transitions.
- Hardware feature level 12.0 is required; no silent software fallback. Debug layer is enabled if installed and its availability is reported.
## Risks / Trade-offs
- Upload-heap geometry is not optimal for large scenes → immutable buffer API allows future default-memory uploads.
- Prototype has 16 contexts and 256 sampled texture descriptors → explicit capacity errors.
- Debug layer may not be installed → report availability, keep readback and lifecycle checks independent.
- GPU stalls/device removal → bounded fence waits and actionable HRESULT diagnostics.
