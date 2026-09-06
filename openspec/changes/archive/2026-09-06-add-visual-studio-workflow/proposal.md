## Why
The user works primarily inside Visual Studio solutions. The current Ninja scripts and a manually generated solution do not provide a repeatable IDE workflow.
## What Changes
- Add a one-command solution generation script and a double-click Windows entry point.
- Discover installed Visual Studio/CMake tools and normalize child-process environment keys to avoid duplicate PATH/Path failures.
- Set the Viewer startup project, show owned headers/shaders/docs and add a dependency-aware CTest project.
- Document IDE and command-line generation, build, test and debugging workflows.
## Capabilities
### New Capabilities
- `visual-studio-workflow`: Reproducible solution generation and IDE build/test entry points.
### Modified Capabilities
None.
## Impact
Windows tooling scripts, CMake IDE metadata/custom test target and README. Renderer behavior remains unchanged.
