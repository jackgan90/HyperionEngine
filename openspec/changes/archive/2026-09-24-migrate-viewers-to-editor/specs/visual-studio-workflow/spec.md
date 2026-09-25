## MODIFIED Requirements

### Requirement: IDE build and test usability
The solution SHALL expose owned headers and shaders, select the Editor as the default startup project and provide a test project that builds its prerequisites before invoking CTest.
#### Scenario: Run all tests in the IDE
- **WHEN** the user builds hyperion_check for Debug or Release
- **THEN** the required executables are built and CTest runs using that configuration, propagating failures to the build.
