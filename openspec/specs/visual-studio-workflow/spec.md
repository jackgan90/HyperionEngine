# visual-studio-workflow Specification

## Purpose
TBD - created by archiving change add-visual-studio-workflow. Update Purpose after archive.
## Requirements
### Requirement: One-command solution generation
The repository SHALL provide script entry points that generate an x64 Visual Studio solution using detected compatible tools and locked dependencies.
#### Scenario: Generate from another working directory
- **WHEN** the generation script is called outside the repository
- **THEN** it resolves the repository location and creates Hyperion.sln in the selected version's dedicated build directory.
#### Scenario: Repeated generation
- **WHEN** the script runs again
- **THEN** the same solution is regenerated without deleting source files or modifying system settings.

### Requirement: IDE build and test usability
The solution SHALL expose owned headers and shaders, select the Viewer as the default startup project and provide a test project that builds its prerequisites before invoking CTest.
#### Scenario: Run all tests in the IDE
- **WHEN** the user builds hyperion_check for Debug or Release
- **THEN** the required executables are built and CTest runs using that configuration, propagating failures to the build.

### Requirement: Documented workflow
The README SHALL explain generation, tool selection, IDE debugging and test execution, including the distinction between CTest and Test Explorer.
#### Scenario: Fresh checkout
- **WHEN** a user follows the documented script command with prerequisites installed
- **THEN** required dependencies are prepared and the solution path and next actions are reported.
