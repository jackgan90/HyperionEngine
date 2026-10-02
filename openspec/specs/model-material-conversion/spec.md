# model-material-conversion Specification

## Purpose

Define explicit source texture role and sampler conversion rules in AssetImport while preserving native material content, product identities, resource sharing and fallback behavior.

## Requirements
### Requirement: Explicit model texture role mapping

AssetImport SHALL own one private description for each supported source texture role, associating its typed source member with texture, sampler and UV semantics, encoding and stable output order. Descriptor declaration order SHALL NOT determine mapping or encoding. The built-in mapping SHALL cover BaseColor and Emissive as sRGB and MetallicRoughness, Normal and Occlusion as Linear, preserving UV0/UV1 and sampler selection.

#### Scenario: Distinct bindings for all roles
- **WHEN** all five roles have distinct image, sampler and UV selections
- **THEN** every native material semantic receives its corresponding source binding and encoding

#### Scenario: Reordered descriptors
- **WHEN** the same valid role descriptions are supplied in a different declaration order
- **THEN** model/material/texture content, product order, product keys, shared keys and encoded revisions are identical

#### Scenario: Invalid private descriptions
- **WHEN** the description contains missing or duplicate output orders, duplicate source members or duplicate target semantics
- **THEN** conversion rejects it before producing output

### Requirement: Explicit source sampler interpretation

Conversion SHALL interpret each supported source Min filter, Mag filter and U/V wrap mode explicitly, without deriving filtering or mip availability from enum ordering. It SHALL preserve existing serialized enum values, material sampler defaults and invalid-enum rejection.

#### Scenario: Supported sampler combinations
- **WHEN** a source sampler uses any legal Min/Mag/U/V combination
- **THEN** its native sampler retains the expected address modes, min/mag/mip filtering, LOD bounds and unchanged remaining defaults

#### Scenario: Invalid sampler
- **WHEN** a source sampler contains an unsupported filter or wrap enum
- **THEN** source conversion fails without publishing assets

### Requirement: Compatible native products and fallback resources

Conversion SHALL preserve historical material entry order and native content revisions for unchanged inputs, material slot identities, numeric factors, queue/state flags and subresource IDs. It SHALL retain existing product keys, source-derived shared keys and image-plus-encoding sharing. Missing images SHALL share the existing Linear white product, missing samplers SHALL use existing defaults, and missing normal maps SHALL retain the existing normal-presence flag.

#### Scenario: One image in color and data roles
- **WHEN** one image is used in multiple sRGB roles and multiple Linear roles
- **THEN** one product per interpretation is emitted and matching interpretations share references without merging sRGB and Linear

#### Scenario: Missing resources and material
- **WHEN** a role lacks an image or sampler, or a primitive has no authored material
- **THEN** conversion retains the existing white/default sampler fallback and creates the same default material and normal-presence values

#### Scenario: Compatible forced reimport
- **WHEN** unchanged source content is imported again through the shared import service
- **THEN** existing root and surviving child identities and content revisions remain compatible, and an ordinary unchanged reimport performs zero writes
