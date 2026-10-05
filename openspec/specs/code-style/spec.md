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
The repository SHALL provide commands for checking formatting and semantic C++ naming without modifying third-party sources. Formatting and naming checks SHALL return failure on violations and document their tooling prerequisites.

The repository SHALL also provide a read-only, Python-only physical file size report through the standard style command and a dedicated size-only mode, and SHALL register that scan with CTest. File size findings SHALL be advisory and SHALL NOT cause failure solely because a file exceeds the recommendation or grows. Invalid exclusion policies and file read errors SHALL still cause failure. Size reporting SHALL require neither LLVM nor a configured C++ build. The existing paths-only mode SHALL retain its path-checking scope.

Repository guidance SHALL distinguish tool coverage from manual review. Automatic file size reporting does not check the 100-line function recommendation, establish cohesion exceptions for functions or files, or establish compliance with the semantic maintainability rules; those matters SHALL be reviewed independently of tool success.

#### Scenario: A formatting or naming violation is introduced
- **WHEN** a contributor runs the relevant formatting or naming check with its required tools installed
- **THEN** the offending owned source is reported and the command returns nonzero

#### Scenario: Report file size without native tooling
- **WHEN** a contributor runs the size-only command with Python available and LLVM and a C++ build absent
- **THEN** all owned source paths are scanned against the file size recommendation and reviewed exclusions without modifying source or policy
- **AND** files exceeding the recommendation are reported without causing a nonzero exit status

#### Scenario: Formatting changes physical file size
- **WHEN** the formatting mode writes formatted source files
- **THEN** it reports the resulting physical file size advisories without changing the exclusion policy or failing solely because of file size

### Requirement: Migration preserves runtime contracts
Style refactoring SHALL update source and build references consistently and preserve existing serialized keys, plugin identifiers, CLI behavior and other external contracts unless an explicitly authorized behavior change provides the necessary migration.

#### Scenario: Validate a behavior-preserving refactor
- **WHEN** owned symbols or files are renamed or responsibilities are split
- **THEN** affected build references are updated and relevant builds and behavior tests verify that existing contracts remain unchanged

### Requirement: Focused function length
Repository guidance SHALL recommend that a single function stay within 100 lines and that longer functions be split at logical boundaries. Counting SHALL cover the complete definition from the first signature line through the closing brace, including blank lines, comments, internal lambdas and constructor definitions. Exceeding the recommendation SHALL be allowed only when the logic is extremely cohesive and meaningful decomposition is difficult. Repository-wide compliance at all times SHALL NOT be required. Comments may clarify non-obvious cohesion, but a dedicated justification comment SHALL NOT be required solely because a function exceeds 100 lines.

#### Scenario: Review an oversized function
- **WHEN** a new or materially changed function exceeds 100 lines including its complete definition
- **THEN** splitting by responsibility is recommended, with an exception allowed only when review establishes that extremely cohesive logic makes meaningful decomposition difficult

#### Scenario: Split a function
- **WHEN** a function is decomposed to satisfy the length principle
- **THEN** the split follows responsibility, data ownership or execution stages, preserves behavior, and does not hide the length through formatting compression or internal lambdas

### Requirement: Recommended non-test C++ file size
Repository guidance SHALL recommend that owned non-test C++ files stay within 1000 physical lines, including includes, blank lines and comments, and that larger files be split at logical boundaries. Exceeding the recommendation SHALL be allowed only when the file's logic is extremely cohesive and meaningful decomposition is difficult. This covers `.cpp`, `.h` and C++ `.inl` files; third-party and automatically generated files are excluded. Test-file status SHALL be determined by actual purpose and build ownership, not naming alone. Test files SHALL be exempt from the file-size recommendation, but their functions SHALL still follow the function-length principle. A function-level cohesion exception SHALL NOT automatically establish a file-level exception.

Repository-wide compliance at all times SHALL NOT be required. Reviews of new files and material changes SHALL assess cohesion and meaningful decomposition. Existing larger files SHOULD be assessed during material changes to the owning module and split incrementally; a local fix need not expand into an unrelated large refactor. Tool success SHALL NOT establish that an oversized file meets the cohesion exception.

The automatic scan SHALL inspect C++ sources under `Source` independent of the selected build configuration, count LF/CRLF physical lines including an unterminated final line, and report a path, line count and review advisory for files exceeding 1000 lines unless an exact reviewed exclusion applies. Test, generated-source and third-party exclusions SHALL record their purpose and ownership evidence; directory names and filename suffixes SHALL NOT grant exclusions. Policy review SHALL verify those facts against actual purpose and build ownership.

The repository SHALL NOT maintain a legacy oversized-production-file inventory, line-count baseline or growth ceilings for this recommendation. File growth, reduction, deletion or rename SHALL NOT require size baseline updates. Invalid, duplicate and missing-source exclusion entries SHALL fail the scan; reviewed exclusion paths SHALL be updated or removed after deletion or rename.

#### Scenario: Split an oversized implementation
- **WHEN** a non-test owned C++ file exceeds 1000 lines and review finds meaningful decomposition possible
- **THEN** splitting along logical responsibilities is recommended while preserving module Public/Private boundaries, interfaces and lifetimes, subject to the documented existing-code migration guidance
- **AND** mechanical chunks, compressed formatting or include fragments do not substitute for meaningful decomposition

#### Scenario: Introduce an oversized unclassified file
- **WHEN** a file without a reviewed exclusion contains 1001 physical lines
- **THEN** the scan reports its path, line count and cohesion review advisory without failing, including when its name or directory suggests a test

#### Scenario: Keep an extremely cohesive implementation together
- **WHEN** review establishes that an oversized function or file has extremely cohesive logic and meaningful decomposition is difficult
- **THEN** exceeding the respective recommendation is allowed, without requiring a line-count baseline

#### Scenario: Resize or retire a production file
- **WHEN** an existing production file grows, shrinks, is deleted or renamed
- **THEN** the scan evaluates the current files and reports applicable advisories without failing because of size or requiring baseline maintenance

#### Scenario: Exclude a reviewed test implementation
- **WHEN** a test implementation has an exact exclusion with purpose and build ownership evidence
- **THEN** it is excluded from the file-size advisories while unrelated production files, including production code also linked into tests, remain scanned

#### Scenario: Reject invalid exclusion data
- **WHEN** an exclusion has an invalid record, duplicate key or path that does not exactly match an existing source file
- **THEN** the scan fails and reports the invalid policy data

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
