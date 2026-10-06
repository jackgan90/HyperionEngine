## ADDED Requirements

### Requirement: Independent recoverable shader storage
Shader compilation SHALL consume the reusable derived-data store using its existing semantic compilation identity. Default application cache paths SHALL be independent of out. Cache location changes SHALL NOT alter semantic shader keys or reflection. Storage failures SHALL permit successful cold compilation results to be used.

#### Scenario: Cold cache after deletion
- **WHEN** the local cache is deleted between runs with valid source/toolchain inputs
- **THEN** shaders recompile with equivalent output/reflection and user configuration remains intact

#### Scenario: Cache relocation
- **WHEN** a valid store is relocated and inputs remain identical
- **THEN** compilation can reuse its records with the same semantic keys
