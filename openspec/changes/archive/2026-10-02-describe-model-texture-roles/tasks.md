## 1. Independent compatibility baseline

- [x] 1.1 Add fixed-source role, sampler, sharing/fallback and rejection tests; capture pre-refactor output fingerprints and run them on the unchanged converter.

## 2. Private conversion contract

- [x] 2.1 Introduce the private role description, validated stable ordering and descriptor-driven conversion while preserving product identity and native content.
- [x] 2.2 Replace ordinal sampler interpretation with explicit Min/Mag/wrap conversion and preserve defaults and rejection behavior.
- [x] 2.3 Add module-private descriptor permutation and invalid-description regression coverage through the production conversion path.

## 3. Documentation and verification

- [x] 3.1 Document role ownership, output ordering and sampler interpretation in the asset pipeline documentation.
- [x] 3.2 Complete Debug/Release builds and affected CPU, publication, D3D12 and GUI/CLI/MCP import regressions.
- [x] 3.3 Complete format, semantic naming, module boundary, diff and strict OpenSpec checks; record validation results without archiving or committing.
