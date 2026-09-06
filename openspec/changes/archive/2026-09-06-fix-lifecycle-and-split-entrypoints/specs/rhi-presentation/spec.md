## ADDED Requirements

### Requirement: Recoverable frame cancellation
RHI SHALL expose idempotent frame cancellation on the coordinator after recording tasks finish. It SHALL retain submitted resources until GPU completion and allow a subsequent frame when cancellation succeeds on a healthy device.

#### Scenario: Cancel an unsubmitted frame
- **WHEN** a begun frame has recorded commands but cannot be submitted
- **THEN** cancellation abandons those commands and the next frame can render normally

#### Scenario: Cancel after submission failure
- **WHEN** frame ending fails after commands were submitted
- **THEN** cancellation drains submitted work before releasing its resources and reports a failure if the device cannot safely recover

#### Scenario: Presentation reports failure while GPU commands remain pending
- **WHEN** Present fails after command submission and successful fence signaling
- **THEN** the frame remains cancellable until submitted work is drained, the original presentation error is reported, and a healthy device can render the next frame
