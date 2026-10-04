# import-product-key-contracts Specification

## Purpose
Define AssetImport-owned shared image and publication identity formats while preserving historical key bytes, custom importer identities and native asset reuse.
## Requirements
### Requirement: Shared image identity has one private format owner

AssetImport SHALL construct and interpret shared-image identities through one private contract with named source, typed encoding and recipe values. Builtin white and the current image recipe SHALL each have one production definition. Existing model product order, keys, shared keys and native content SHALL remain unchanged.

#### Scenario: Existing model textures
- **WHEN** model conversion emits external sRGB/Linear textures or its builtin white fallback
- **THEN** emitted keys exactly match fixed legacy bytes including `rgba8-full-mips-v1` and `builtin/white-rgba8-v1`

#### Scenario: Legacy and opaque shared keys
- **WHEN** publication receives legacy image syntax, an unfamiliar recipe, or a custom opaque shared key
- **THEN** source rewriting, delimiter precedence, fallback prefix replacement and local-path rejection retain their previous behavior without narrowing the accepted custom-key protocol

### Requirement: Publication identity uses named inputs and preserves persisted bytes

AssetImport SHALL centrally format source, named-product and shared-product publication keys from distinct named inputs. Root identity and the texture-content setting SHALL each have a single production definition. Public importer records, reflected schemas and persistence versions SHALL remain unchanged.

#### Scenario: Fixed publication samples
- **WHEN** existing logical sources, product names, custom shared keys and type IDs are formatted
- **THEN** the results exactly match legacy `source/`, `product/`, `shared/` and `$root` samples including their separators

### Requirement: Historical publication identities remain reusable

Reimport SHALL retain live root/product IDs and output mappings from historical native assets. Cross-root shared products, content-based texture reuse, conflicts, source validation and atomic publication SHALL retain their existing behavior.

#### Scenario: Seeded historical OutputIds
- **WHEN** an import encounters pre-existing native assets with fixed legacy OutputIds and IDs, including named, source, image and opaque custom products
- **THEN** initial and forced reimport preserve those exact IDs and mappings without relying on texture-content deduplication

#### Scenario: Shared library publication
- **WHEN** independent roots use a common product identity or incompatible contents claim one identity
- **THEN** publication preserves existing sharing or reports the existing controlled conflict before committing incompatible writes
