#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "SceneInstanceInternal.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
FSceneSkyData LoadSky(FAssetService& InAssets, FTaskSystem& InTasks, FRenderResourceService& InResources,
                      const FAssetRef& InReference, const std::filesystem::path& InContaining,
                      const FCancellationToken& InCancellation)
{
	InCancellation.Check();
	if (InReference.Id.empty() && InReference.Revision.empty())
	{
		// An explicit unpinned path selection requests the current root, including a repaired asset.
		InAssets.Invalidate(InAssets.Resolve(InReference, InContaining));
	}
	const auto Root = InAssets.LoadReferenceAsync(InReference, InContaining).Get(InTasks);
	const auto Sky = Root->As<FSkyAsset>();
	FSceneSkyData Result;
	Result.Name = Sky->Name;
	Result.Reference = {Root->Header.Id, PathToUtf8(Root->Path), Root->Header.TypeId, Root->Header.Revision};
	Result.Irradiance = Sky->Irradiance;
	std::map<FAssetRef, std::shared_ptr<const FTextureAsset>> Textures;
	FMaterialAssetValues Values;
	for (const auto& Reference : {Sky->Radiance, Sky->Specular, Sky->Brdf})
	{
		InCancellation.Check();
		const auto Loaded = InAssets.LoadReferenceAsync(Reference, Root->Path).Get(InTasks);
		const auto Texture = Loaded->As<FTextureAsset>();
		const bool bCube = Values.size() < 2;
		if (Texture->Dimension != (bCube ? ETextureDimension::Cube : ETextureDimension::Texture2D) ||
		    Texture->Encoding != EMaterialTextureEncoding::Linear || Texture->Format == ETextureFormat::Rgba8Unorm)
		{
			throw std::runtime_error(
			    "Sky products require linear floating-point radiance/specular cubes and a 2D BRDF LUT");
		}
		const FAssetRef Canonical{Loaded->Header.Id, PathToUtf8(Loaded->Path), Loaded->Header.TypeId,
		                          Loaded->Header.Revision};
		Textures.emplace(Canonical, Texture);
		Result.Dependencies.push_back(Canonical);
		FMaterialAssetValue Value;
		Value.Type =
		    FMaterialParameterType::Resource(bCube ? EMaterialValueKind::TextureCube : EMaterialValueKind::Texture2D);
		Value.Texture = Canonical;
		Values.push_back({std::to_string(Values.size()), std::move(Value)});
	}
	const auto Prepared = InResources.PrepareAssetValues(Values, Textures);
	for (std::size_t Index = 0; Index < Prepared.size(); ++Index)
	{
		Result.Textures[Index] = Prepared[Index].Value.Texture;
	}
	InCancellation.Check();
	return Result;
}
} // namespace

void FSceneInstance::FImpl::PollSky(FSkyLoad& InLoad)
{
	if (InLoad.bComplete || !InLoad.Preparation.Ready())
	{
		return;
	}
	try
	{
		if (!InLoad.Data)
		{
			InLoad.Data = InLoad.Preparation.GetReady();
		}
		if (InLoad.bGpuSubmitted)
		{
			if (!InLoad.Upload.Ready())
			{
				return;
			}
			if (*InLoad.Upload.GetReady())
			{
				auto Light = *Scene.FindEnvironmentLight(InLoad.Handle);
				Light.Data = InLoad.Data;
				Light.Sky->Id = InLoad.Data->Reference.Id;
				Light.Sky->Revision = InLoad.Data->Reference.Revision;
				InLoad.Reference = *Light.Sky;
				Scene.SetEnvironmentLight(InLoad.Handle, std::move(Light));
				InLoad.bComplete = true;
				return;
			}
		}
		InLoad.Upload = DispatchAsync<bool>(Tasks, {EDomain::Rhi, 0},
		                                    [Endpoint = Session.GetResources().GetPreparation(), Data = InLoad.Data]
		                                    {
			                                    return Endpoint.PrepareTextures(Data->Textures, Data);
		                                    });
		InLoad.bGpuSubmitted = true;
	}
	catch (const std::exception& Failure)
	{
		InLoad.Error = Failure.what();
		InLoad.bComplete = true;
	}
}

void FSceneInstance::FImpl::PollSkies()
{
	for (auto It = SkyLoads.begin(); It != SkyLoads.end();)
	{
		const auto* Light = Scene.FindEnvironmentLight(It->first);
		if (!Light || Light->Source != ESceneEnvironmentSource::SkyAsset || Light->Sky != It->second->Reference)
		{
			It->second->Cancellation.Cancel();
			RetiredSkyLoads.push_back(It->second);
			It = SkyLoads.erase(It);
		}
		else
		{
			++It;
		}
	}
	std::erase_if(RetiredSkyLoads,
	              [](const auto& InLoad)
	              {
		              return InLoad->Preparation.Ready() && (!InLoad->bGpuSubmitted || InLoad->Upload.Ready());
	              });
	Status.PendingSkies = 0;
	Status.FailedSkies = 0;
	for (const auto Handle : Scene.GetNodes(ESceneNodeKind::EnvironmentLight))
	{
		const auto& Light = *Scene.FindEnvironmentLight(Handle);
		if (Light.Source != ESceneEnvironmentSource::SkyAsset)
		{
			continue;
		}
		auto& Load = SkyLoads[Handle];
		if (!Load)
		{
			Load = std::make_shared<FSkyLoad>();
			Load->Handle = Handle;
			Load->Reference = *Light.Sky;
			RegisterSceneAssetTypes(Assets.Types());
			Load->Preparation = DispatchAsync<FSceneSkyData>(
			    Tasks, {EDomain::Worker},
			    [this, Reference = Load->Reference, Containing = Path, Cancellation = Load->Cancellation]
			    {
				    return LoadSky(Assets, Tasks, Session.GetResources(), Reference, Containing, Cancellation);
			    },
			    Load->Cancellation);
		}
		PollSky(*Load);
		Status.PendingSkies += !Load->bComplete;
		Status.FailedSkies += !Load->Error.empty();
	}
}

void FSceneInstance::FImpl::CloseSkies()
{
	for (auto& [Handle, Load] : SkyLoads)
	{
		RetiredSkyLoads.push_back(std::move(Load));
	}
	SkyLoads.clear();
	for (auto& Load : RetiredSkyLoads)
	{
		Load->Cancellation.Cancel();
		for (const auto Task : {Load->Preparation.Task(), Load->Upload.Task()})
		{
			try
			{
				Tasks.Wait(Task);
			}
			catch (...)
			{
			}
		}
	}
	RetiredSkyLoads.clear();
}

std::string FSceneInstance::GetSkyStatus(FSceneHandle InHandle) const
{
	Impl->Tasks.Require({EDomain::Main});
	const auto It = Impl->SkyLoads.find(InHandle);
	if (It == Impl->SkyLoads.end())
	{
		return "No sky requested";
	}
	const auto& Load = *It->second;
	return !Load.Error.empty() ? "Failed: " + Load.Error
	       : Load.bComplete    ? "Ready"
	       : Load.Data         ? "Uploading"
	                           : "Loading";
}
} // namespace Hyperion
