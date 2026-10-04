## MODIFIED Requirements

### Requirement: Repeatable style verification
The repository SHALL provide commands for checking formatting and semantic C++ naming without modifying third-party sources. Checks SHALL return failure on violations and document their tooling prerequisites.

The repository SHALL also provide a read-only, Python-only physical file size check through the standard style command and a dedicated size-only mode, and SHALL register that check with CTest. Size checking SHALL require neither LLVM nor a configured C++ build. The existing paths-only mode SHALL retain its path-checking scope.

Repository guidance SHALL distinguish tool coverage from manual review. Automatic file size checking does not check the 100-line function principle or establish compliance with the semantic maintainability rules; those rules SHALL be reviewed independently of tool success.

#### Scenario: A style violation is introduced
- **WHEN** a contributor runs the relevant style check with its required tools installed
- **THEN** the offending owned source is reported and the command returns nonzero

#### Scenario: Check file size without native tooling
- **WHEN** a contributor runs the size-only command with Python available and LLVM and a C++ build absent
- **THEN** all owned source paths are checked against the file size policy without modifying source or policy

#### Scenario: Formatting changes physical file size
- **WHEN** the formatting mode writes formatted source files
- **THEN** it checks the resulting physical file sizes and returns nonzero for any size policy violation without changing the policy

### Requirement: Bounded non-test C++ files
Owned non-test C++ files SHALL contain no more than 500 physical lines, including includes, blank lines and comments. This covers `.cpp`, `.h` and C++ `.inl` files; third-party and automatically generated files are excluded. Test-file status SHALL be determined by actual purpose and build ownership, not naming alone. Test functions SHALL still follow the function-length principle, and its tightly coupled logic exception SHALL NOT exempt a file from the file limit.

New files SHALL meet the limit. Existing oversized files SHALL be assessed during material changes to the owning module and split incrementally; a local fix need not expand into an unrelated large refactor, but deferred oversized files SHALL have their scope and follow-up splitting work recorded in review.

The automatic check SHALL scan C++ sources under `Source` independent of the selected build configuration, count LF/CRLF physical lines including an unterminated final line, and apply the limit unless an exact reviewed policy entry applies. Test, generated-source and third-party exclusions SHALL record their purpose and ownership evidence; directory names and filename suffixes SHALL NOT grant exclusions. Policy review SHALL verify those facts against actual purpose and build ownership.

Existing oversized production files SHALL have a reviewed baseline recording their current line count and follow-up splitting responsibility. The check SHALL reject growth, require reducing a baseline count when its file shrinks, and require removing an entry once its file meets the limit. Invalid, duplicate, overlapping and missing-source policy entries SHALL fail the check. Policy updates SHALL NOT raise ceilings or add new oversized production files to bypass the limit; reviewed renames SHALL preserve or reduce the existing debt.

#### Scenario: Split an oversized implementation
- **WHEN** a non-test owned C++ file exceeds 500 lines
- **THEN** it is split along logical responsibilities while preserving module Public/Private boundaries, interfaces and lifetimes, subject to the documented existing-code migration rule
- **AND** mechanical chunks, compressed formatting or include fragments do not substitute for meaningful decomposition

#### Scenario: Introduce an oversized unclassified file
- **WHEN** a file without a reviewed exclusion or baseline contains 501 physical lines
- **THEN** the check reports its path and line count and fails, including when its name or directory suggests a test

#### Scenario: Grow a legacy file
- **WHEN** an existing oversized production file exceeds its recorded count
- **THEN** the check fails and reports the recorded ceiling without updating it

#### Scenario: Reduce or retire legacy debt
- **WHEN** a baseline file shrinks or is deleted or renamed
- **THEN** the check requires updating or removing its stale entry and a reviewed rename retains the existing ceiling or a lower one

#### Scenario: Exclude a reviewed test implementation
- **WHEN** a test implementation has an exact exclusion with purpose and build ownership evidence
- **THEN** it is excluded from the file limit while unrelated production files, including production code also linked into tests, remain checked
