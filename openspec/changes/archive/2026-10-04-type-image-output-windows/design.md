## Context

`FImageOutputRequest::Window` defaults to `main` and reflects as an unrestricted string. Editor checks general busy state, then supported windows, then asset-window availability, then destination validation. Unknown strings currently round trip in Reflection and are rejected only at service admission. Pending requests retain their window through frame routing, polling and asset-window closure.

## Goals / Non-Goals

**Goals:** Typed known window identity throughout native screenshot flow; lossless protocol values; unchanged validation precedence, routes, completion and failure behavior.

**Non-Goals:** RenderDoc exercise options, screenshot UI, new window destinations, capture scheduling, GPU readback, plugin lifecycle or new automation operations.

## Decisions

1. Renderer owns `EImageOutputWindow` (Main/Assets) and a small `FImageOutputWindow` value with private variant storage. Its default is explicitly Main. One mapping defines the two wire tokens; unknown values are retained verbatim and have no known kind. This follows the existing local diagnostic compatibility pattern. A strict reflected enum would change the open schema and reject unknown strings before existing busy checks; a parallel cached enum/string pair would duplicate state.
2. `window` keeps its existing string shape, default, description, persistence, version and member association through a local reflection projection. Its read callback retains opaque strings; service admission still owns rejection. This does not create a generic Reflection feature.
3. Editor's request handler checks busy first, obtains the known kind, rejects absence with the existing message, then follows its existing asset availability and PrepareImageOutput order. Main rendering, asset closure and polling compare the typed kind. PendingImage ownership and every completion/error path remain unchanged.
4. PrepareImageOutput continues only destination normalization/overwrite checks and copying the request. In particular, its standalone behavior does not gain window validation. GUI, native clients and automation retain the same shared services; no transport adapter changes are required.

## Risks / Trade-offs

- Open string compatibility needs a tagged value → private single storage, fixed-token wire/archive regressions, default and known/opaque transition checks.
- Earlier validation could change error precedence → keep existing control flow and test invalid target versus invalid path and absent asset window in JSONL and MCP against baseline binaries first.
- Routing or shutdown regressions → exercise actual main/asset PNG output, retain and inspect pending state through native completion, and review all polling/close consumers.

## Migration Plan

Capture current operation/type contracts and error behavior, implement types/consumers, build and run focused regressions, compare contracts, obtain independent audit and stop for user diff acceptance. No persisted-data migration is needed.
