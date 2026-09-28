## REMOVED Requirements

### Requirement: Shared default sky scene action
**Reason**: Sky lights are now ordinary placeable objects with an Engine Cloudy default, and the sky asset is edited through the generic Details picker.
**Migration**: Place a Sky Light from Place Object, add a Sky Light component, or create a node with an environment component through `scene.node.create`. Select a different sky through the component's Sky asset property.
