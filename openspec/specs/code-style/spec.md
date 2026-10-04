# code-style Specification

## Purpose
Define the repository's default Unreal-inspired source conventions, code-size and semantic maintainability rules, interoperability exceptions, repeatable checks, and behavior-preserving migration requirements.
## Requirements
### Requirement: Default engine coding conventions
Owned C++ and HLSL SHALL use the documented Unreal-inspired conventions: PascalCase identifiers, UE type prefixes, PascalCase source filenames, Allman braces and four-column tabs. Repository guidance SHALL record interoperability exceptions. Boolean variables, including atomic boolean flags, SHALL use lowercase `b` followed by PascalCase; boolean parameters SHALL use `bIn`/`bOut`, and query functions SHALL remain PascalCase.

#### Scenario: Contributor adds an engine module
- **WHEN** a contributor reads repository guidance and creates a module
- **THEN** the applicable naming, file layout and formatting rules are available with examples and checked-in formatter/naming configurations

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

### Requirement: Migration preserves runtime contracts
Style refactoring SHALL update source and build references consistently and preserve existing serialized keys, plugin identifiers, CLI behavior and other external contracts unless an explicitly authorized behavior change provides the necessary migration.

#### Scenario: Validate a behavior-preserving refactor
- **WHEN** owned symbols or files are renamed or responsibilities are split
- **THEN** affected build references are updated and relevant builds and behavior tests verify that existing contracts remain unchanged

### Requirement: Focused function length
Repository guidance SHALL state that a single function normally does not exceed 100 lines and longer functions are split at logical boundaries. Counting SHALL cover the complete definition from the first signature line through the closing brace, including blank lines, comments, internal lambdas and constructor definitions. An exception SHALL be allowed when the logic is so tightly coupled that meaningful decomposition is difficult. Comments may clarify non-obvious coupling, but a dedicated justification comment SHALL NOT be required solely because a function exceeds 100 lines.

#### Scenario: Review an oversized function
- **WHEN** a new or materially changed function exceeds 100 lines including its complete definition
- **THEN** it is split by responsibility or review establishes that tightly coupled logic makes meaningful decomposition difficult

#### Scenario: Split a function
- **WHEN** a function is decomposed to satisfy the length principle
- **THEN** the split follows responsibility, data ownership or execution stages, preserves behavior, and does not hide the length through formatting compression or internal lambdas

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

### Requirement: Stable semantic identity
Behavior SHALL use stable typed identities or explicit states independently of display text and presentation order. String representations SHALL be parsed and validated at input, persistence or protocol boundaries; unknown values SHALL be rejected according to the contract, and existing external identifiers SHALL remain stable unless explicitly migrated.

#### Scenario: Change a display label
- **WHEN** a label, translation or presentation order changes
- **THEN** the underlying operation or state retains its identity and behavior

### Requirement: One authoritative domain definition
Each shared domain fact SHALL have one authoritative definition owned by its domain. Consumers SHALL obtain alternate representations through queries, explicit conversions or generation rather than separately maintained layouts, masks, offsets or catalogs. Layer-specific validation SHALL retain its responsibilities while using those authoritative facts.

#### Scenario: Extend a shared definition
- **WHEN** a shared field layout or operation definition changes
- **THEN** consumers derive their representations from the owner and retain the validation required by their own boundaries

### Requirement: Named typed relationships
Related data and choices SHALL use named typed records, discriminated types or explicit mappings rather than relationships hidden in bare numbers, positional or parallel arrays, enum ordinals or untyped payloads. Required protocol numbers and external layouts SHALL have authoritative meanings, mappings and valid ranges; merely naming a constant does not resolve an implicit relationship.

#### Scenario: Add a command or mapped value
- **WHEN** a command payload or cross-representation mapping is extended
- **THEN** its meaning and associated data are explicit without requiring callers to infer them from unrelated positions or numeric coincidences

### Requirement: Explicit domain-owned policies
Independent policies SHALL be modeled explicitly and validated and executed by their owning domain, rather than inferred from names or incidentally correlated state. GUI, CLI and automation adapters SHALL convert inputs and present results while sharing domain validation, transactions, history, persistence and side effects.

#### Scenario: Expose an operation through another adapter
- **WHEN** an existing domain operation is exposed through another user or agent entry point
- **THEN** the entry point uses the same domain workflow and explicit policy values instead of implementing a second decision path

### Requirement: Complete semantic dependencies
Caching, reuse, invalidation and notifications SHALL depend on the actual semantic inputs affecting the result. Changes SHALL be normalized before semantic comparison determines the necessary effects. Adding a field, policy or registered type SHALL include review of the complete dependency chain rather than relying solely on a hand-maintained built-in whitelist.

#### Scenario: Extend an input affecting a cached result
- **WHEN** a new semantic input changes a cached result or required notification
- **THEN** the dependency chain accounts for it without missing updates or rebuilding and notifying for semantically unchanged state
