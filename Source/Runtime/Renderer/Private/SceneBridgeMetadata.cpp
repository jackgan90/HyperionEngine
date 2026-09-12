#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneBridge.h"

namespace Hyperion
{
std::shared_ptr<const FSceneMetadata> FSceneRenderBridge::PrepareMetadata(
    const std::vector<FSceneChange>& InChanges) const
{
	auto Result = Metadata ? std::make_shared<FSceneMetadata>(*Metadata) : std::make_shared<FSceneMetadata>();
	Result->Token = {Scene.GetIdentity(), AttachmentEpoch, Metadata ? Metadata->Token.PublicationSerial + 1 : 1,
	                 Scene.GetRevision()};
	Result->Settings = Scene.GetSettings();
	for (const auto& Change : InChanges)
	{
		if (Change.Kind == ESceneNodeKind::Camera)
		{
			if (Change.bRemoved)
			{
				Result->Cameras.erase(Change.Handle);
			}
			else if (Change.Node)
			{
				Result->Cameras[Change.Handle] = {*Change.Node->Camera, ExtractScenePose(Change.World),
				                                  Change.bEffectiveEnabled};
			}
		}
		else if (Change.Kind == ESceneNodeKind::DirectionalLight)
		{
			if (Change.bRemoved)
			{
				Result->DirectionalLights.erase(Change.Handle);
			}
			else if (Change.Node)
			{
				Result->DirectionalLights[Change.Handle] = {*Change.Node->DirectionalLight,
				                                            ScaleVector(ExtractScenePose(Change.World).Forward, -1),
				                                            Change.bEffectiveEnabled};
			}
		}
		else if (Change.Kind == ESceneNodeKind::EnvironmentLight)
		{
			if (Change.bRemoved)
			{
				Result->EnvironmentLights.erase(Change.Handle);
			}
			else if (Change.Node)
			{
				Result->EnvironmentLights[Change.Handle] = {*Change.Node->EnvironmentLight, Change.bEffectiveEnabled};
			}
		}
	}
	return Result;
}

void FSceneRenderBridge::ObserveScene(bool bInWait)
{
	for (auto It = SceneReceipts.begin(); It != SceneReceipts.end();)
	{
		if (!bInWait && !It->Ready())
		{
			++It;
			continue;
		}
		try
		{
			Tasks.Wait(*It);
		}
		catch (const std::exception& Error)
		{
			SceneError = Error.what();
		}
		catch (...)
		{
			SceneError = "Unknown scene publication failure";
		}
		It = SceneReceipts.erase(It);
		++StatusRevision;
	}
}

FScenePublicationToken FSceneRenderBridge::GetToken() const
{
	Tasks.Require({EDomain::Main});
	if (!Metadata || bClosed || !SceneError.empty())
	{
		throw std::logic_error("Scene publication is uninitialized, closed or failed");
	}
	return Metadata->Token;
}

FTaskHandle FSceneRenderBridge::GetReceipt() const
{
	Tasks.Require({EDomain::Main});
	return LatestReceipt;
}

std::string FSceneRenderBridge::GetSceneError() const
{
	Tasks.Require({EDomain::Main});
	return SceneError;
}
} // namespace Hyperion
