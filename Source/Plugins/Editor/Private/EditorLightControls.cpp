#include "EditorApplication.h"

namespace Hyperion
{
FSceneLightingInfo FEditorPlugin::LightingInfo()
{
	if (!Scene || Scene->GetStatus().bClosed)
	{
		throw FSceneEditError(SceneEditErrors::Unavailable, "No scene is open");
	}
	return Scene->GetLightingInfo();
}
} // namespace Hyperion
