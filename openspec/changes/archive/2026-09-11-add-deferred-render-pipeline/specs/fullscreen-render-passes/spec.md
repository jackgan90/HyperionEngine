## ADDED Requirements

### Requirement: Renderer-owned fullscreen triangle passes
Renderer SHALL provide reusable fullscreen pass construction over the existing oversized triangle, with explicit shaders, bindings, sampled inputs, output attachments, viewport/scissor, graphics state and pass names. Lighting, tonemapping and depth preview SHALL share this mechanism. Resource preparation SHALL occur on RHI 0 under existing ownership and cache rules without primitive collection or per-frame geometry uploads.

#### Scenario: Lighting followed by output and preview
- **WHEN** those fullscreen operations appear in one graph
- **THEN** they reuse triangle geometry, declare all sampled dependencies and render into their specified targets/regions

#### Scenario: Frozen fullscreen frame
- **WHEN** the next frame changes settings or target size before an earlier frame finishes
- **THEN** the earlier pass continues using its owned immutable parameters and retained resources
