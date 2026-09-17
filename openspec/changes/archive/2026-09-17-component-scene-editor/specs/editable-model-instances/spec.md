## ADDED Requirements

### Requirement: Stable editable model instances
Model-to-scene imports SHALL create one model object referencing the complete model. Scene import, native upgrade and runtime loading SHALL preserve authored topology without automatically expanding source nodes or primitives. Instance placement and material overrides SHALL remain scene-owned; internal model transforms and geometry SHALL remain asset-owned. Source node and section identities SHALL be stable within an asset revision and preserved across compatible rebuilds. Render bindings SHALL preserve object/component/subresource provenance without requiring one-to-one draw identity.

#### Scenario: Shared mesh independent instances
- **WHEN** two scene objects reference the same model and one instance is transformed or receives a material override
- **THEN** only the selected instance changes and geometry remains shared

#### Scenario: Import a multi-primitive model
- **WHEN** a model with multiple nodes or primitives is imported as a scene
- **THEN** one model object references the complete model and no internal nodes or primitives become independent scene objects

#### Scenario: Upgrade an already edited expanded scene
- **WHEN** a scene containing independently edited primitive child objects is loaded or upgraded
- **THEN** the authored children and overrides remain intact and no implicit collapse discards edits

#### Scenario: Reuse an older import cache
- **WHEN** a model-to-scene output was produced under the automatic expansion policy
- **THEN** the changed publication policy invalidates the cache and explicit reimport produces a whole-model reference

### Requirement: Explicit asset graph migration
Migration SHALL preserve world transforms, effective visibility, material overrides, scene camera/light selections and stable existing asset IDs. It SHALL publish valid dependency revisions before scene roots, update reproducible source/import paths and never rewrite assets merely by loading them.

#### Scenario: Upgrade Sponza
- **WHEN** the existing Sponza scene and model graph are explicitly upgraded
- **THEN** its verified unedited model subtree becomes one whole-model reference, shared dependency data is preserved and matched-camera rendering remains equivalent with a reloadable native dependency graph

#### Scenario: Unsafe compact migration
- **WHEN** an expanded subtree contains child edits that cannot be represented by the whole-model instance
- **THEN** compact migration rejects that subtree instead of silently losing authored state
