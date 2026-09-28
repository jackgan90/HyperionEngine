## ADDED Requirements

### Requirement: Deterministic component priorities
Sky and directional components SHALL expose a reflected signed 32-bit Priority integer defaulting to zero. Highest eligible priority SHALL win, with lexical persistent object ID as the final tie breaker. Negative values SHALL be valid. Derived winners SHALL NOT be authored or create history entries.

#### Scenario: Equal priorities and reload
- **WHEN** eligible lights have equal priority and registration order changes across save/reload
- **THEN** the same persistent object wins and a highest-priority tie is observable

#### Scenario: Candidate retirement
- **WHEN** a winner is disabled through its object or ancestor, or removed
- **THEN** the next eligible candidate wins and undo restores the previous outcome

### Requirement: Quiet contextual light inspection
The Editor SHALL replace activation buttons and persistent active/ready text with Priority component properties. Highest-priority ties SHALL be indicated at Priority; winner information SHALL be available on hover. Sky asset readiness SHALL be silent when ready and loading or failure SHALL be indicated at the Sky asset property, including dependency and upload failures.

#### Scenario: Failed sky resource
- **WHEN** the highest-priority sky requests a missing or invalid resource
- **THEN** the asset field reports the failure without selecting a lower-priority sky

#### Scenario: Effective and overridden sky hover
- **WHEN** the user hovers Sky Light Priority help
- **THEN** the effective sky shows a green `Current effective environment` on the second line, and another enabled sky shows a red `Environment overridden by <display name>` on the second line followed by advice to increase Priority above the other sky lights
- **AND** neither sky tooltip exposes persistent object IDs or tie-breaking details; disabled skies retain their disabled explanation and selection rules remain unchanged

#### Scenario: Highest-priority warning hover
- **WHEN** the user hovers the Priority `[!]` marker on a sky or directional light
- **THEN** its independent tooltip explains that multiple eligible lights share the highest Priority, without exposing persistent object IDs or tie-breaking rules
- **AND** the adjacent `(?)` help remains a separate hover target for status and advice

#### Scenario: Directional shadow source hover
- **WHEN** the user hovers Directional Light Priority help
- **THEN** the selected shadow source shows a green `Current directional shadow source`, and another eligible directional light shows a red `Directional shadows overridden by <display name>` followed by an explanation that direct lighting remains active and advice to increase Priority above the other shadow-casting directional lights
- **AND** disabled or shadow-ineligible lights retain their exclusion explanation without misleading Priority advice, and no directional tooltip exposes persistent object IDs or tie-breaking details

### Requirement: Current automation parity
Automation SHALL expose the same priority/component edits, validation, undo/redo and resolved lighting/asset diagnostics as GUI through shared domain contracts. Obsolete light.main.get/set and authored scene light selections SHALL be removed from current discovery/schema.

#### Scenario: Discovery and invocation
- **WHEN** an agent discovers light component schemas, edits priority and queries resolved lighting
- **THEN** the winner and diagnostics match GUI, invalid edits have no side effects and undo restores priority
