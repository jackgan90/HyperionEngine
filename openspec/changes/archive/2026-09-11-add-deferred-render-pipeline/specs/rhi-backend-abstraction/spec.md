## ADDED Requirements

### Requirement: Explicit color formats and multiple targets
RHI SHALL expose vendor-independent color formats, sampled render-color creation, immutable texture format information and enabled format/MRT capabilities. Graphics pipeline target signatures SHALL describe each ordered color slot. D3D12 SHALL bind and validate all declared RTVs, reflect pixel-output compatibility, and retain existing ownership and failure checks. Complete target signatures SHALL participate in pipeline and draw-cache equality.

#### Scenario: Mixed GBuffer formats
- **WHEN** one pass writes RGBA8 and RGBA16F attachments
- **THEN** the backend creates matching views/PSO formats and accepts compatible shader outputs without converting data to sRGB

#### Scenario: Mismatched pipeline or foreign target
- **WHEN** a draw declares the wrong target format/count or an attachment belongs to another device
- **THEN** validation rejects it before GPU submission
