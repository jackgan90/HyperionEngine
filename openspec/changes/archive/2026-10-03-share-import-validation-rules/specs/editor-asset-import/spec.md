## ADDED Requirements

### Requirement: Shared import input rules with workspace admission
The import workspace SHALL share dependency-library normalization, asset-output extension, source/output separation and source-identity rules with the independent publication service. It SHALL retain its existing Main, generation, Game-root, read-only, text-bound, importer and RootId admission, error codes and messages. GUI and automation SHALL continue to use the same workspace operation contracts.

#### Scenario: Invalid portable source identity
- **WHEN** an import request supplies only one of sourceRoot/sourceId, or sourceId contains a colon, backslash, leading slash or any double-dot substring
- **THEN** workspace validation rejects it with the existing invalid_arguments diagnostic and performs no writes

#### Scenario: Default library and root boundary
- **WHEN** a request omits its dependency library
- **THEN** validation uses the normalized output parent as library and still enforces the current writable Game root for both paths

#### Scenario: Workspace grouped admission remains stricter
- **WHEN** a grouped workspace request supplies an explicit library, even the output parent
- **THEN** validation rejects it with the existing grouped-import diagnostic without publication
