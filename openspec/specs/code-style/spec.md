# code-style Specification

## Purpose
Define the repository's default Unreal-inspired source conventions, interoperability exceptions, repeatable style checks, and behavior-preserving migration requirements.
## Requirements
### Requirement: Default engine coding conventions
Owned C++ and HLSL SHALL use the documented Unreal-inspired conventions: PascalCase identifiers, UE type prefixes, PascalCase source filenames, Allman braces and four-column tabs. Repository guidance SHALL record interoperability exceptions and the owner's uppercase-first boolean rule.

#### Scenario: Contributor adds an engine module
- **WHEN** a contributor reads repository guidance and creates a module
- **THEN** the applicable naming, file layout and formatting rules are available with examples and checked-in formatter/naming configurations

### Requirement: Repeatable style verification
The repository SHALL provide commands for checking formatting and semantic C++ naming without modifying third-party sources. Checks SHALL return failure on violations and document their tooling prerequisites.

#### Scenario: A style violation is introduced
- **WHEN** a contributor runs the relevant style check with its required tools installed
- **THEN** the offending owned source is reported and the command returns nonzero

### Requirement: Migration preserves runtime contracts
The style migration SHALL update source and build references consistently and preserve existing serialized keys, plugin identifiers and CLI behavior.

#### Scenario: Build and run after renaming
- **WHEN** the Visual Studio solution is regenerated and its Debug and Release tests are built and run
- **THEN** the core, wrapper, shader, GUI and DX12 triangle acceptance tests continue to pass
