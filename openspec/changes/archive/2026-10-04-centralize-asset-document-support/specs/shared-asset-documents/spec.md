## ADDED Requirements

### Requirement: Shared base asset document support
AssetEditing SHALL own the base document support decision for canonical model, material, texture and sky record types. Editor and standalone automation open adapters SHALL consume that decision instead of maintaining separate type lists. Preview capabilities and field editing policies SHALL remain independently owned, and scene documents SHALL keep their existing scene workflow.

#### Scenario: Supported and unsupported assets
- **WHEN** either adapter opens a canonical model, material, texture or sky asset, or encounters a scene or unknown record type
- **THEN** both agree on base support while retaining their existing errors, asynchronous completion and workspace entry lifecycle

#### Scenario: Preview capability remains independent
- **WHEN** an asset is accepted as a CPU document but its preview cannot be prepared
- **THEN** the adapter retains its existing preview failure behavior without changing the shared support rule, history or persistence

#### Scenario: Automation and GUI compatibility
- **WHEN** existing asset operations are discovered and invoked or the same document is opened through GUI
- **THEN** operation IDs, schemas, revision checks, dirty state, history, saves and close/drain behavior remain compatible
