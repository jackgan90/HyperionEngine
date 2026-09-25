## MODIFIED Requirements

### Requirement: Shared capture orchestration
Editor and independent runtime consumers SHALL reuse engine-owned full-frame capture scope and replay logic, preserving RHI 0 execution, cancellation on exceptions, optional service lifetime and RenderDoc-disabled build isolation.

#### Scenario: Independent capture consumer
- **WHEN** a runtime consumer requests a capture with automatic opening enabled
- **THEN** it uses the same begin/end/cancel/replay implementation as Editor and retains its existing capture behavior

#### Scenario: Build without RenderDoc
- **WHEN** RenderDoc support is compiled out
- **THEN** Editor and independent runtime consumers compile and run without RenderDoc headers, library or DLL requirements
