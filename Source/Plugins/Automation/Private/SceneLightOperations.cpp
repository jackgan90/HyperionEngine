#include "Hyperion/Renderer/SceneLightControls.h"
#include "SceneOperations.h"

namespace Hyperion
{
void RegisterSceneLightControls(FOperationCatalog& InCatalog, ISceneLightControls* InLights)
{
	FOperationInfo Info;
	Info.Id = "scene.lighting.get";
	Info.Owner = "automation-scene";
	Info.Summary = "Read resolved lighting and asset diagnostics";
	Info.Description = "Read priority winners, highest-priority ties, effective enablement and sky asset status. "
	                   "Edit priority and light properties through scene.component.<type>.set; all enabled directional "
	                   "lights illuminate and one eligible light supplies shadows.";
	Info.Effects = "Reads scene light state without modifying history.";
	Info.Completion = "Main snapshot.";
	Info.bReadOnly = true;
	Info.Unavailable = InLights ? "" : "Scene lighting diagnostics provider unavailable.";
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FSceneLightingInfo>(Info,
	                                                                        [InLights](const auto&)
	                                                                        {
		                                                                        return InLights->LightingInfo();
	                                                                        }));
}
} // namespace Hyperion
