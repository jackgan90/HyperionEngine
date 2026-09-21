## 1. Native discovery and current references

- [x] 1.1 Add bounded native metadata reads and rebuildable ID discovery with duplicate diagnostics.
- [x] 1.2 Replace startup and content-root catalog requirements with discovery.
- [x] 1.3 Persist current-content references and refresh loading caches at explicit reload boundaries.

## 2. Portable native publication

- [x] 2.1 Discover current products without an authoritative asset-library file; preserve same-target identities and portable provenance.
- [x] 2.2 Publish visible stable filenames, reuse shared resources, and skip unchanged imports.
- [x] 2.3 Stage the complete graph and roll back synchronous publication failures.
- [x] 2.4 Remove internal-asset browser filtering and update integration contracts.

## 3. Existing content migration

- [x] 3.1 Capture an untouched baseline and implement native-only migration of reachable assets and standalone roots.
- [x] 3.2 Stage HyperionAssets migration, consolidate public aliases, remove revision constraints and stale management state.
- [x] 3.3 Validate staged graphs and scene behavior, then publish the verified migration without restoring absent historical assets.

## 4. Verification and documentation

- [x] 4.1 Cover identity, relocation, sharing, unchanged reimport, rollback, discovery, and current-content reload regressions.
- [x] 4.2 Update asset/import/content documentation and optional sample recipes.
- [x] 4.3 Complete style, build, targeted tests, final native validation, and OpenSpec validation; leave both repositories uncommitted.
- [x] 4.4 Complete the explicitly requested independent quality audit, reproduce and repair publication/rename findings, and obtain targeted re-review.
