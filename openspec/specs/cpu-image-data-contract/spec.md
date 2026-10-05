# cpu-image-data-contract Specification

## Purpose
Define independent CPU image value ownership and service-independent RHI image contracts while preserving existing image processing and presentation behavior.

## Requirements
### Requirement: Independent image value ownership
CPU image dimensions, RGBA storage and color encoding SHALL have one standard-library-only definition independent of Assets services, codecs, Renderer and native backends. Existing public type names, fields, defaults, aggregate initialization and ownership behavior MUST remain compatible.

#### Scenario: Existing Assets consumer
- **WHEN** a consumer includes Assets.h and constructs an image
- **THEN** it uses the same ImageData-owned type without duplicate definitions or changed image values

#### Scenario: Vendor dependency on ImageData
- **WHEN** ImageData declares a link to a codec or other third-party target
- **THEN** configured graph collection fails with the declaration location instead of reporting an independent module

### Requirement: Service-independent RHI image contract
RHI public image upload/readback contracts SHALL depend on image data directly and MUST NOT reach Assets through their configured production link closure. Architectural validation SHALL reject reintroduced direct or transitive Assets dependencies.

#### Scenario: RHI-only public consumer
- **WHEN** an executable links only hyperion_rhi and includes its device/swapchain contracts
- **THEN** image upload and capture result signatures compile and link without Assets include or library propagation

#### Scenario: Indirect regression
- **WHEN** RHI links a module that reaches Assets
- **THEN** target validation reports the dependency chain

#### Scenario: Non-owned wrapper hides an owned dependency
- **WHEN** a non-owned wrapper links an owned target, directly or through a generator expression or link property
- **THEN** graph collection rejects the unsupported dependency with the wrapper identity and declaration location, rather than silently omitting the edge

### Requirement: Preserve image processing and presentation
Decoding, encoding and image file IO SHALL remain in Assets. Image dimensions, channel order, linear/sRGB encoding, floating point values, uploads, readbacks and existing screenshot operations MUST preserve their behavior and protocol.

#### Scenario: Codec and rendered image regression
- **WHEN** existing PNG/EXR round trips and GPU upload/readback/screenshot paths run
- **THEN** existing precision, quantization, dimensions, encoding and output checks pass with unchanged serialized keys and operation identities
