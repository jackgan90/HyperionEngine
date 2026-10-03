## ADDED Requirements

### Requirement: Explicit mode controls instance preparation and consumption
Material compilation SHALL derive `HYP_ENABLE_INSTANCE`, record reflection mapping, capacity and system instance-ID validation from execution mode. Batch eligibility, instance packing, native preparation and draw selection SHALL consume typed instance selection without interpreting variant names. Ordinary session drawing SHALL retain its `Default` identity and require Ordinary mode. Optional instance generation SHALL preserve existing default identities and failure isolation; it SHALL NOT overwrite a same-named Ordinary variant. Existing batch compatibility, coverage, capacity, fallback and GPU retirement rules SHALL remain unchanged.

#### Scenario: Automatic instance preparation
- **WHEN** an Ordinary `Default` pass declares instance arrays and no explicit instance candidate exists
- **THEN** preparation attempts an explicitly Instanced `Instance` candidate with the ordinary defines, retaining the valid ordinary program on failure

#### Scenario: Automatic identity collision
- **WHEN** an Ordinary variant already owns the `Instance` identity
- **THEN** optional generation reports the conflict without changing that variant's mode or replacing it

#### Scenario: Renamed instance candidate reaches the GPU
- **WHEN** a valid instance candidate uses a custom name
- **THEN** batch lookup, cached and uncached record packing, native preparation and drawing use that candidate and produce the same covered pixels as ordinary draws

#### Scenario: Optional failure and retained frames
- **WHEN** optional instance preparation is unsupported or a new frame changes instance data
- **THEN** ordinary fallback remains available and previously recorded frames retain their original resources and output
