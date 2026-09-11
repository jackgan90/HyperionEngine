## MODIFIED Requirements

### Requirement: Unified asynchronous loading
The engine SHALL convert registered external formats through one asynchronous import service and obtain all source bytes through engine IO. Runtime Assets SHALL load only native reflected assets; external sources SHALL first be imported.

#### Scenario: Unified asynchronous loading acceptance
- **WHEN** a glTF references external geometry and a JPEG
- **THEN** the import service creates a complete CPU model and native output without a vendor opening files

### Requirement: Explicit format capabilities
The engine SHALL support reflected native read/write and report unsupported source features or external writes. Source conversion SHALL belong to AssetImport; the native asset service SHALL not register glTF or scene JSON codecs.

#### Scenario: Explicit format capabilities acceptance
- **WHEN** a caller attempts glTF export or imports an unsupported required extension
- **THEN** the operation returns a clear capability diagnostic instead of discarding data

### Requirement: Retry failed asset requests
A fresh source conversion or native load SHALL retry a completed failed cached request while successful cached results and in-flight requests continue to be shared. Previously returned failed requests SHALL retain their errors. Explicit reimport SHALL account for changed source dependencies.

#### Scenario: Repair a missing dependency
- **WHEN** conversion fails, the missing file is supplied, and callers convert the same path and type again
- **THEN** the new conversion succeeds without clearing unrelated cached results and duplicate retries share the new work
