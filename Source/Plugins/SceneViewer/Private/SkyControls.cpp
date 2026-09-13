#include "Hyperion/SceneViewer/SkyControls.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
FSkyControlResult DrawSkyAssetControls(FGui& InGui, FSceneEnvironmentLight& InLight, std::string& InPath,
                                       std::span<const std::string> InChoices)
{
	FSkyControlResult Result;
	std::vector<std::string> Labels{InLight.Data ? InLight.Data->Name : "Choose a sky asset..."};
	std::size_t Selection{};
	for (const auto& Choice : InChoices)
	{
		Labels.push_back(PathToUtf8(PathFromUtf8(Choice).filename()));
		if (InLight.Sky && InLight.Sky->Path == Choice)
		{
			Selection = Labels.size() - 1;
		}
	}
	InGui.Text("Sky asset (.hasset)");
	const bool bSelected = InGui.Combo("##SkyAsset", Labels, Selection) && Selection > 0;
	Result.AssetBounds = InGui.LastItemBounds();
	if (bSelected)
	{
		InPath = InChoices[Selection - 1];
	}
	InGui.Text("Custom sky hasset path");
	InGui.InputText("##SkyPath", InPath, false);
	Result.PathBounds = InGui.LastItemBounds();
	const bool bApply = InGui.Button("Apply sky asset", !InPath.empty());
	Result.ApplyBounds = InGui.LastItemBounds();
	if (bSelected || bApply)
	{
		InLight.Sky = FAssetRef{"", PathToUtf8(std::filesystem::absolute(PathFromUtf8(InPath)).lexically_normal()),
		                        RecordType<FSkyAsset>().Id, ""};
		InLight.Source = ESceneEnvironmentSource::SkyAsset;
		Result.bChanged = true;
	}
	std::size_t Source = static_cast<std::size_t>(InLight.Source);
	const std::array<std::string, 2> Sources{"Constant color", "Sky asset"};
	InGui.Text("Environment source");
	if (InGui.Combo("##SkySource", Sources, Source))
	{
		InLight.Source = static_cast<ESceneEnvironmentSource>(Source);
		Result.bChanged = true;
	}
	InGui.Text("Sky / environment intensity");
	Result.bChanged |= InGui.InputFloat("##SkyIntensity", InLight.Intensity);
	InGui.Text("Sky yaw (radians)");
	Result.bChanged |= InGui.InputFloat("##SkyYaw", InLight.YawRadians);
	Result.bChanged |= InGui.Checkbox("Show sky background", InLight.bVisible);
	Result.VisibilityBounds = InGui.LastItemBounds();
	return Result;
}
} // namespace Hyperion
