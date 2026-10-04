## Context

At `ff08895`, ModelImport constructs `image/<source>/srgb|linear/<recipe>` shared identities and AssetLibrary parses them when replacing a physical source with its logical source identity. Publication wraps identities as `product/<source>/<product>|<type>`, `shared/<shared-key>|<type>` or `source/<source>|<type>` and persists them in Import.OutputIds. Custom importers can supply arbitrary SharedKey strings. These two layers cannot be collapsed into a closed four-field public record.

## Goals / Non-Goals

**Goals:** One AssetImport-private owner of shared-image syntax and publication formatting; named inputs that expose source/product/type associations; exact compatibility with existing keys and reimport behavior.

**Non-Goals:** New importer APIs, escaping/versioning, changing portable-source validation, public reflection, new automation operations, content deduplication changes, or publication lifecycle/transaction changes. Editor observation and remaining roadmap items belong to later changes.

## Decisions

1. Add a private ImportProductKey contract. A shared image record has Source, typed material texture Encoding and Recipe. The formatter and permissive legacy parser share the encoding delimiters. The default full-mips recipe and builtin white key have one definition.
2. Retain legacy parsing: prefer the last `/srgb/` delimiter when present, otherwise the last `/linear/`; preserve unknown/empty recipe text. Even unusual delimiter placement retains the old substring behavior. Unknown forms remain opaque and follow the existing generic physical-prefix replacement and local-path rejection. Tightening this grammar would require a separate compatibility migration.
3. Give source, named-product and shared-product publication formatting distinct named records and overloads. Do not add an outer parser: the unescaped legacy grammar cannot uniquely separate arbitrary source/product strings, and existing consumers compare these outer keys as opaque values. Keep the root marker and texture-content setting with this private owner.
4. Retain the existing public string fields, FPublication path policy, ID selection, content hashing, revision checks and write transaction. Model conversion and publication call the contract at the existing boundaries. No GUI/agent adapter is necessary because no user capability or reflected operation changes.
5. Test the actual publication implementation with fixed old OutputIds and fixed IDs in pre-existing native files. Run these public tests against the unchanged production code first. Add private format/parser/portability edge coverage, retain the independent literal oracles in model tests, and run existing cross-root texture reuse/conflict/rollback and CLI integration tests after the refactor.

## Risks / Trade-offs

- A formatter/parser round trip could reproduce a new incompatible format → fixed historical literals and preseeded OutputIds are the primary oracles.
- Over-parsing custom keys could reject a working importer → preserve opaque fallback and verify a custom key containing separators, alongside builtin image forms.
- Deduplication can hide ID-selection regressions → seed distinct non-texture products with known IDs, so texture-content reuse cannot rescue a wrong key.
- Internal refactoring could change validation order or asynchronous ownership → leave source policy and publication control flow in place; test affected integration paths and inspect the call chain.

## Migration Plan

No data migration or version bump. Build and test, freeze the diff for independent quality audit, verify any findings and obtain user acceptance before archive/commit. Rollback is a source revert; assets remain readable with the previous implementation.

## Open Questions

None within the compatibility-preserving scope.
