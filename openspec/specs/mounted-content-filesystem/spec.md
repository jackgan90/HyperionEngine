# mounted-content-filesystem Specification

## Purpose
Define immutable content mounts, portable package paths, filesystem access validation and application configuration.
## Requirements
### Requirement: Mounted package file access
IO SHALL provide frozen mounts mapping virtual roots to local roots, normalized UTF-8 package paths, reads, enumeration, atomic writes and leases. It SHALL reject unknown roots, root traversal, physical aliases and write attempts on read-only mounts. Explicit local tool/test access SHALL remain available without interpreting unknown virtual paths as local files.

#### Scenario: Relocated checkout
- **WHEN** Engine and Game physical roots change and mount configuration is updated
- **THEN** the same package paths load without content edits or dependence on the process working directory

#### Scenario: Invalid access
- **WHEN** a request escapes a mount, uses an unknown root or writes through a read-only mount
- **THEN** it fails before accessing the target storage

### Requirement: Application composition
Applications SHALL configure mounts before asset/shader requests and resolve relative mount locations against the configuration file. Runtime content browsing and persistence SHALL expose package paths.

#### Scenario: Custom content location
- **WHEN** Viewer uses a mount configuration outside the repository and selects a sky
- **THEN** sky enumeration, loading and saved references use the configured virtual namespace
