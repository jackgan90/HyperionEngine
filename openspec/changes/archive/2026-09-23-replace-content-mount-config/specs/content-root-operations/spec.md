## ADDED Requirements

### Requirement: Shared content root transactions
The engine SHALL expose a Main-owned CPU service for querying, setting and clearing the Game asset root. It SHALL validate and index candidates before modifying active content, preserve Engine, reject stale generations and coordinate registered content consumers. Same canonical directory and permissions SHALL preserve existing documents. All participants SHALL pass dirty/busy preflight before any old content is released. Dirty documents SHALL require saving first or explicit discard; accepted pending edits/saves SHALL complete before switching. Successful changes SHALL invalidate old document handles and cached content.

#### Scenario: Invalid or blocked transition
- **WHEN** a candidate is invalid, the generation is stale, a participant is busy or dirty documents lack discard authorization
- **THEN** the root and all existing documents remain unchanged and a controlled error is returned

#### Scenario: Switch and clear
- **WHEN** a valid transition is committed after consumers release old content
- **THEN** new requests use the selected root or report Game unmounted, old handles cannot edit new content, and Engine remains available

### Requirement: Discoverable root automation
The automation catalog SHALL register content.root.get, content.root.set and content.root.clear with reflected input/output schemas, descriptions, effects and examples. An unmounted Game root SHALL leave these operations available. CLI/JSONL/MCP SHALL invoke the same service as the Editor. Disabled or failed asset providers SHALL report unavailable operations without breaking unrelated discovery.

#### Scenario: Agent selects its own environment
- **WHEN** an agent starts MCP without a Game root, queries the current generation, sets a directory and opens an asset
- **THEN** the workflow succeeds without any mount configuration file or Editor preference dependency

#### Scenario: Process-local state
- **WHEN** a persistent session changes its root and a separate single-shot process starts
- **THEN** the separate process starts without that root unless explicitly supplied for its invocation
