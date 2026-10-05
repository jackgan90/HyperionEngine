## Why

Typed scene component get/set/batch adapters are private templates in Automation's central built-in registration file. A component's owning feature cannot reuse those domain operations without editing the central concrete type list.

## What Changes

- Expose a typed scene component operation registration adapter in AutomationHost with explicit owner and read/write exposure.
- Keep Scene/SceneEditing independent of concrete plugins and execute all edits through existing validation, transactions, revisions and history.
- Preserve built-in operation IDs, schemas and behavior while allowing a feature-owned test component to register and discover operations independently.
- Document startup/seal, duplicate handling, provider lifetime and owner withdrawal; test affected lifecycle and GUI/domain equivalence.

## Capabilities

### New Capabilities

- `owned-component-operation-registration`: Reusable typed registration with owner/exposure and existing domain editing semantics.

### Modified Capabilities

None. Existing component operations retain their public behavior.

## Impact

Automation plugin public adapters, built-in component registration, SceneEditing regression consumers, capability coverage and extension documentation. No transport type branches, component storage rewrite or runtime plugin loading is introduced.
