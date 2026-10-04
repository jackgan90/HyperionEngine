## ADDED Requirements

### Requirement: Named resource retry identity
Resource cache keys SHALL explicitly name immutable identity, version, configuration and retry generation. Retry lookup SHALL compare the request prefix and advance only the retry generation, preserving sharing, failure recovery and retained-resource lifetime.

#### Scenario: Retry failed production
- **WHEN** a compatible request follows failed production
- **THEN** lookup retains the same request identity and uses the next existing retry generation without changing successful leases
