## Why

Editor users need to select an asset workspace without editing mount configuration, browse its directories, and open scenes discovered from disk. Switching the meaning of `/Game` must retire old document and resource state before admitting new requests.

## What Changes

- Remove the fixed HYPERION menu label; add File > Open... and Recent (five successful roots).
- Restore the last root at startup, with explicit mount configuration taking precedence.
- Add a directory tree and immediate-child icon grid with native folder selection, refresh, and optional internal asset visibility.
- Discover scenes from the active root by native asset type, independently of Catalog; preserve Open Scene and route double-click through the same document action.
- Add a quiescent content transition that protects unsaved edits, drains work, retires GPU references and replaces mounts/catalog/cache together.

## Capabilities

### New Capabilities
- `editor-content-browser`: Root selection, persistence, browsing, scene discovery and protected content transitions.

### Modified Capabilities
- `mounted-content-filesystem`: Permit explicit quiescent replacement of frozen mount sets and directory-entry enumeration.
- `scene-editor`: Discover openable scenes under the active Game root rather than from a catalog list.

## Impact

Runtime IO, Assets, Platform and Gui; Renderer resource retirement; ApplicationServices asset ownership; Editor state, preferences, menus and document actions; focused tests and documentation. Static plugin selection and native asset formats remain unchanged. No Git commit is requested.
