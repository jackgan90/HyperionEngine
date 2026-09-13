#include "Hyperion/IO/Path.h"
#include "Hyperion/SceneViewer/SkyControls.h"
#include "SceneViewerInternal.h"
#include <algorithm>

namespace Hyperion
{
void FSceneViewerPlugin::FImpl::DrawSkyControls(FGui& InGui)
{
	const auto Handle = Scene.GetSettings().EnvironmentLight;
	const auto* Node = Handle ? Scene.FindNode(*Handle) : nullptr;
	if (!Node || !Node->EnvironmentLight)
	{
		return;
	}
	try
	{
		auto Light = *Node->EnvironmentLight;
		InGui.Text("Sky / active environment");
		const bool bRefresh = InGui.Button("Refresh sky assets");
		if (!bSkyChoicesInitialized || bRefresh)
		{
			SkyChoices.clear();
			const auto Folder = Path.parent_path().parent_path() / "Skies";
			if (std::filesystem::is_directory(Folder))
			{
				for (const auto& Entry : std::filesystem::directory_iterator(Folder))
				{
					if (Entry.is_regular_file() && Entry.path().extension() == ".hasset")
					{
						SkyChoices.push_back(PathToUtf8(std::filesystem::absolute(Entry.path()).lexically_normal()));
					}
				}
			}
			std::sort(SkyChoices.begin(), SkyChoices.end());
			bSkyChoicesInitialized = true;
		}
		if (DrawSkyAssetControls(InGui, Light, SkyPath, SkyChoices).bChanged)
		{
			Scene.SetEnvironmentLight(*Handle, std::move(Light));
		}
		InGui.TextWrapped(Scene.GetSkyStatus(*Handle));
		if (const auto* Current = Scene.FindNode(*Handle); Current->EnvironmentLight->Sky)
		{
			const auto& Requested = *Current->EnvironmentLight->Sky;
			const auto& Active = Current->EnvironmentLight->Data;
			InGui.TextWrapped(Active ? "Active sky: " + Active->Name : "Active: awaiting sky asset");
			if (!Active || Active->Reference.Id != Requested.Id || Active->Reference.Revision != Requested.Revision)
			{
				InGui.TextWrapped("Requested: " + Requested.Path);
			}
		}
	}
	catch (const std::exception& Failure)
	{
		EditError = Failure.what();
		InGui.TextWrapped(EditError);
	}
}
} // namespace Hyperion
