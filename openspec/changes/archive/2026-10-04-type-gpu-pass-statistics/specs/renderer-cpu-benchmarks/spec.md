## ADDED Requirements

### Requirement: Typed GPU pass attribution
Renderer SHALL own explicit GPU pass categories and category-local instance identifiers independently of display names. Built-in benchmark contributors SHALL declare their metadata at pass construction. Benchmark aggregation SHALL use only recognized typed metadata, preserve existing CSV column names/order/precision and category membership, and include every completed pass in the total exactly once. All Deferred lighting variants SHALL share the lighting bucket. Unclassified or unrecognized metadata SHALL contribute only to the total; an out-of-range shadow instance SHALL contribute to shadow total without indexing a cascade slot. Existing GPU submission matching and incomplete-capture rejection SHALL remain unchanged.

#### Scenario: Rename a classified pass
- **WHEN** classified graphics or compute pass names change without changing timing metadata and durations
- **THEN** category and per-cascade benchmark values remain identical

#### Scenario: Misleading unclassified name
- **WHEN** an unclassified or foreign-tagged pass has a former reserved prefix such as Forward or Shadow cascade
- **THEN** only the overall pass total includes its duration

#### Scenario: Graph batch expansion
- **WHEN** a graphics declaration expands into multiple command batches or compute work uses deferred preparation
- **THEN** each emitted command preserves the declared category and instance regardless of name suffix or preparation path

#### Scenario: Existing built-in workload
- **WHEN** the existing built-in timing fixture is exported with explicit metadata
- **THEN** its CSV bytes remain equal to the checked-in oracle and GPU frame identity rules remain enforced
