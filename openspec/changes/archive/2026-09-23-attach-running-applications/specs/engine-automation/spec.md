## ADDED Requirements

### Requirement: Explicit target routing
CLI, JSONL and MCP SHALL expose a small fixed set of target discovery/connect/disconnect methods and optional connection routing on existing bootstrap methods. A CLI attach option SHALL establish a default target. Requests without an attached target SHALL preserve standalone behavior. Attached discovery and invocation SHALL use the target's authoritative catalog and schemas without frontend domain registration.

#### Scenario: Existing MCP configuration
- **WHEN** a client launches with --mcp and connects to a subsequently started application through target management
- **THEN** it can describe and call that application's operations using the returned connection ID

#### Scenario: Lost target
- **WHEN** an explicitly selected target terminates
- **THEN** subsequent calls report disconnection without falling back to standalone or another target

#### Scenario: Frontend and target paths
- **WHEN** a CLI loads parameters using --json-file and invokes an operation containing a domain path
- **THEN** the parameter file is read on the frontend and the domain path is interpreted on the selected target
