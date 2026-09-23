#include "SceneViewerInternal.h"
#include <cmath>

namespace Hyperion
{
TAsyncResult<bool> FSceneViewerPlugin::SaveAsync(const std::filesystem::path& InPath)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	P.Document.PollSave();
	auto Result = P.Document.Save(PathToUtf8(InPath));
	P.SaveStatus = "Saving scene...";
	return Result;
}

const std::string& FSceneViewerPlugin::SaveStatus() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->SaveStatus;
}

void FSceneViewerPlugin::FImpl::PollSave()
{
	Document.PollSave();
	if (const auto Outcome = Document.TakeSaveOutcome())
	{
		SaveStatus = Outcome->bSucceeded ? "Saved scene: " + Outcome->Path : "Scene save failed: " + Outcome->Error;
	}
}

void FSceneViewerPlugin::FImpl::DrawSave(FGui& InGui)
{
	if (InGui.Button("Save edited scene"))
	{
		auto Destination = Path;
		Destination.replace_extension(".edited.hasset");
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
