# async-gltf-import Specification

## Purpose
Load supported static glTF assets through a shared asynchronous asset service using engine-controlled IO, and persist native reflected assets.
## Requirements
### Requirement: Unified asynchronous loading
The engine SHALL load registered asset types through one asynchronous service and obtain all source bytes through engine IO.

#### Scenario: Unified asynchronous loading acceptance
- **WHEN** a glTF references external geometry and a JPEG
- **THEN** the complete CPU model resolves those dependencies without a vendor opening files

### Requirement: Static glTF semantics
The engine SHALL preserve supported primitive attributes, materials, root selection and node instancing and expand supported accessor encodings.

#### Scenario: Static glTF semantics acceptance
- **WHEN** a GLB contains sparse attributes and multiple transformed primitives
- **THEN** the resulting validated model matches the specified values and transforms

### Requirement: Shared request lifecycle
The engine SHALL share duplicate loads while isolating consumer cancellation and expose terminal failures.

#### Scenario: Shared request lifecycle acceptance
- **WHEN** two consumers load a source and one cancels
- **THEN** the remaining consumer can complete successfully

### Requirement: Explicit format capabilities
The engine SHALL support reflected native read/write and report unsupported format features or writes.

#### Scenario: Explicit format capabilities acceptance
- **WHEN** a caller attempts glTF export or loads an unsupported required extension
- **THEN** the service returns a clear capability diagnostic instead of discarding data
