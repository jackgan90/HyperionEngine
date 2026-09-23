## Why

Mount configuration files duplicate the editor's selected asset root and make headless automation depend on an asset environment before it can select one. Content roots should be explicit session state managed through the same domain operation for GUI and agents.

## What Changes

- **BREAKING** Remove ContentMounts.json, its local override, JSON loader and --mounts arguments. Preserve virtual package paths and filesystem validation.
- Initialize built-in /Engine content independently; leave /Game unmounted by default.
- Restore Editor roots from preferences and allow explicit directory arguments for tools and standalone applications.
- Extract a CPU content-root service with shared validation, participant coordination, dirty/busy rejection, document retirement and set/clear/query operations.
- Expose root operations through the existing typed automation catalog; retain live sessions and support a per-invocation asset-root convenience argument.
- Migrate tools, regression fixtures, specifications and developer documentation.

## Capabilities

### New Capabilities

- `content-root-operations`: Shared session root transitions and discoverable automation operations.

### Modified Capabilities

- `mounted-content-filesystem`: Programmatic configuration and independent engine resource location replace mount configuration files.
- `editor-content-browser`: Empty initial root, preference restoration and shared content transition participation.

## Impact

Runtime/Content, IO, ApplicationServices, Editor, Viewer, Automation, AssetTool, integration scripts, CMake, documentation and affected regression coverage. Existing assets retain their format, identities and package references. Builds and executable names remain stable. No commit or archive is requested.
