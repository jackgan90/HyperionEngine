## Why

Editor close tests infer semantic progress from CloseStatus strings even though the transition owner already exposes ClosePhase. The text projection merges Idle/AwaitingDecision/AwaitingSavePath and Ready/Reconfirming, so current assertions cannot distinguish these states.

## What Changes

- Use existing typed close phases in transition unit tests and content GUI acceptance, preserving their existing error and side-effect checks.
- Check waiting-for-path, decision and reconfirmation phases explicitly.
- Keep one focused test for the existing public status text projection through reachable close transitions.

## Capabilities

### New Capabilities

- `editor-close-observation-contracts`: Semantic close assertions use the owner's exact phase while external status text remains compatible.

### Modified Capabilities

None. Existing close behavior and public application.close contracts remain unchanged.

## Impact

Two Editor test/acceptance files and OpenSpec artifacts. No production state, API/schema, lifecycle, persistence, build target or module dependency changes. This is the close-observation portion of roadmap item 15d; preview/save observations and Editor ownership changes remain separate stages.
