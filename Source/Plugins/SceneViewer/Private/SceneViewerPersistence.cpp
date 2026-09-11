#include "SceneViewerInternal.h"
#include <cmath>

namespace Hyperion
{
TAsyncResult<bool> FSceneViewerPlugin::SaveAsync(const std::filesystem::path& InPath)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.Save && !P.Save->Ready())
	{
		throw std::logic_error("Scene save is already in progress");
	}
	if (!P.Manifest)
	{
		throw std::runtime_error("Scene has not loaded");
	}
	auto Snapshot = P.Scene.Snapshot(InPath);
	const FVec3 Direction{std::sin(P.Yaw) * std::cos(P.Pitch), std::sin(P.Pitch), std::cos(P.Yaw) * std::cos(P.Pitch)};
	Snapshot.Eye = Add(P.Target, ScaleVector(Direction, P.Distance));
	Snapshot.Target = P.Target;
	Snapshot.Near = P.Manifest->Near;
	Snapshot.Far = P.Manifest->Far;
	P.Save = P.Assets.SaveAsync(InPath, std::make_shared<const FSceneManifest>(std::move(Snapshot)));
	P.SavePath = InPath;
	P.SaveStatus = "Saving scene...";
	return *P.Save;
}

const std::string& FSceneViewerPlugin::SaveStatus() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->SaveStatus;
}

void FSceneViewerPlugin::FImpl::PollSave()
{
	if (!Save || !Save->Ready())
	{
		return;
	}
	try
	{
		(void)Save->GetReady();
		SaveStatus = "Saved scene: " + SavePath.generic_string();
	}
	catch (const std::exception& Failure)
	{
		SaveStatus = "Scene save failed: " + std::string(Failure.what());
	}
	Save.reset();
}

void FSceneViewerPlugin::FImpl::DrawSave(FGui& InGui)
{
	if (InGui.Button("Save edited scene"))
	{
		auto Destination = Path.parent_path() / (Path.stem().string() + ".edited.hasset");
		try
		{
			Owner->SaveAsync(Destination);
		}
		catch (const std::exception& Failure)
		{
			SaveStatus = "Scene save failed: " + std::string(Failure.what());
		}
	}
	if (!SaveStatus.empty())
	{
		InGui.TextWrapped(SaveStatus);
	}
}
} // namespace Hyperion
