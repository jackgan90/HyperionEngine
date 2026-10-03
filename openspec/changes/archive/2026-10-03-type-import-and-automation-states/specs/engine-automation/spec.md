## ADDED Requirements

### Requirement: Typed session job lifecycle

Automation sessions SHALL own a typed job state independent of import states and protocol text. Existing running/completed/failed/cancelled status tokens, polling envelopes, cancellation capability, bounded retention and shutdown admission/drain semantics SHALL remain unchanged.

#### Scenario: Deferred operation outcomes
- **WHEN** a deferred operation completes, throws or is successfully cancelled
- **THEN** the job exposes the corresponding existing terminal status and outcome, is no longer pending and is not polled again

#### Scenario: Retention and stop admission
- **WHEN** completed jobs are evicted or admission is stopped
- **THEN** running jobs remain tracked, expired IDs report not_found and accepted jobs retain their existing completion/drain behavior
