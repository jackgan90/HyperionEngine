## ADDED Requirements

### Requirement: Owned GUI drag and drop
Gui SHALL expose engine-owned copied payloads, source activation, target hover/preview, delivery and cancellation without leaking ImGui types. Payload identity SHALL survive source widget drawing order and viewport focus SHALL NOT be required to accept a compatible drag. Delivery SHALL occur at most once for a gesture. Esc or focus loss SHALL terminate the drag.

#### Scenario: Payload ownership and delivery
- **WHEN** a source publishes a temporary string payload and the pointer enters and releases on a compatible target in a later frame
- **THEN** the target receives intact copied data once while incompatible targets do not accept it

#### Scenario: Cancellation
- **WHEN** a drag is cancelled before delivery
- **THEN** subsequent frames cannot deliver its stale payload or retain pointer capture

### Requirement: Clipped textured overlays
Gui SHALL support sized image widgets and textured image overlays through engine texture IDs, preserving clip rectangles and application scale without exposing graphics handles. GuiRenderer SHALL retain referenced image resources for submitted frames.

#### Scenario: Light sprite at viewport edge
- **WHEN** a textured marker overlaps the edge of a docked viewport
- **THEN** only its viewport portion is drawn and neighboring panels are unaffected
