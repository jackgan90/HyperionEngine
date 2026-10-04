## ADDED Requirements

### Requirement: Explicit discrete viewport identities
Renderer SHALL associate existing culling and selection-outline typed identities with explicit stable wire values and display labels in one authoritative mapping. Culling SHALL retain 0=None, 1=Linear and 2=Bvh; outline overlap SHALL retain 0=Union and 1=PerObject. Boundary conversion and supported-value validation SHALL use these mappings rather than enum ordinals or numeric maxima. The contract SHALL require no active GUI, renderer instance or plugin service.

#### Scenario: Fixed wire meanings
- **WHEN** an existing culling or outline wire value is parsed and applied
- **THEN** it selects the same documented typed behavior and encodes to the original numeric value

#### Scenario: Unknown numeric or typed choice
- **WHEN** a caller supplies an unmapped wire value or typed identity
- **THEN** conversion rejects it explicitly without narrowing, wrapping or selecting a fallback

### Requirement: Presentation-independent selection
The Editor SHALL select culling and outline choices by their mapped identities, independently of label order. Presentation validation SHALL allow reordering and nonempty relabeling while rejecting missing, duplicate or unsupported identities and changed wire mappings. GUI controls SHALL invoke the existing shared viewport service.

#### Scenario: Reorder or relabel options
- **WHEN** the presentation order or labels change
- **THEN** reading the current selection and choosing an option preserve the same identity and wire meaning

#### Scenario: Ambiguous presentation
- **WHEN** a presentation repeats an identity, omits a supported choice or changes its wire mapping
- **THEN** presentation validation rejects it

### Requirement: Compatible viewport service behavior
Existing operation/type IDs, versions, schema member order and descriptions, scalar kinds, integer ranges, optional/null fields, defaults and invalid-input classification SHALL remain unchanged. Unsupported controls SHALL retain controlled unavailable errors. GUI and automation SHALL share preflight and commit behavior; invalid patches SHALL leave all viewport state unchanged. Culling and outline edits SHALL remain temporary view changes without modifying scene revision, history, dirty state or render-settings revision. Rendering algorithms, frame ownership and resource retirement SHALL remain unchanged.

#### Scenario: Existing clients discover and invoke controls
- **WHEN** clients use api.search, api.describe and types.describe followed by view.get and view.set
- **THEN** they observe the existing complete external shapes and the same valid numeric choices

#### Scenario: Reject a mixed invalid patch
- **WHEN** a patch contains a valid temporary edit and an unsupported or unmapped culling/outline value
- **THEN** the service rejects the patch before applying any edit, preserving existing error precedence

#### Scenario: Equivalent GUI and automation edits
- **WHEN** a user chooses a culling or outline entry and an agent applies the corresponding fixed wire value
- **THEN** the shared state exposes the same choice while unrelated options and document state remain unchanged
