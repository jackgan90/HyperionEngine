## ADDED Requirements

### Requirement: Retry failed asset requests
A fresh load SHALL retry a completed failed cached request while successful cached results and in-flight loads continue to be shared. Previously returned failed requests SHALL retain their errors.

#### Scenario: Repair a missing dependency
- **WHEN** a load fails, the missing file is supplied, and callers load the same path and type again
- **THEN** the new load succeeds without clearing unrelated cached assets and duplicate retries share the new load
