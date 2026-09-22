#include "Hyperion/Renderer/RenderSession.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
namespace
{
void CollectTextures(const FMaterialValue& InValue,
                     std::vector<std::shared_ptr<const FMaterialTextureSource>>& OutTextures)
{
	if (InValue.Texture)
	{
		OutTextures.push_back(InValue.Texture);
	}
	for (const auto& Element : InValue.Elements)
	{
		CollectTextures(Element, OutTextures);
	}
}
} // namespace

void FSceneInstance::FImpl::PrepareRefreshResources(std::shared_ptr<const FAssetRefresh> InData)
{
	FRefreshResources Prepared;
	Prepared.Data = std::move(InData);
	auto& Resources = Session.GetResources();
	for (const auto& [Id, Data] : Prepared.Data->Models)
	{
		Prepared.Geometry.push_back(Resources.RequestModel(Data));
		for (const auto& Material : Data->MaterialSnapshots)
		{
			Prepared.Materials.push_back(Resources.RequestMaterial(Material));
		}
	}
	const auto PrepareSelection = [&](const FSceneMaterialSelection& InSelection)
	{
		if (InSelection.Snapshot)
		{
			Prepared.Materials.push_back(Resources.RequestMaterial(InSelection.Snapshot));
		}
		for (const auto& Value : InSelection.Overrides)
		{
			CollectTextures(Value.Value, Prepared.Textures);
		}
	};
	for (const auto& Selection : Prepared.Data->Selections)
	{
		PrepareSelection(Selection.PreparedSurface);
		for (const auto& [Section, Value] : Selection.PreparedSections)
		{
			PrepareSelection(Value);
		}
	}
	RefreshResources = std::move(Prepared);
	UploadRefreshTextures();
}

void FSceneInstance::FImpl::UploadRefreshTextures()
{
	auto& Prepared = *RefreshResources;
	Prepared.Upload = DispatchAsync<bool>(
	    Tasks, {EDomain::Rhi, 0},
	    [Endpoint = Session.GetResources().GetPreparation(), Textures = Prepared.Textures, Lifetime = Prepared.Data]
	    {
		    return Endpoint.PrepareTextures(Textures, Lifetime);
	    });
}

bool FSceneInstance::FImpl::AreRefreshResourcesReady()
{
	const auto& Prepared = *RefreshResources;
	if (!Prepared.Upload.Ready())
	{
		return false;
	}
	if (!*Prepared.Upload.GetReady())
	{
		UploadRefreshTextures();
		return false;
	}
	bool bReady = true;
	for (const auto& Resource : Prepared.Geometry)
	{
		if (Resource->GetStatus() == ERenderResourceStatus::Failed)
		{
			throw std::runtime_error(Resource->GetError());
		}
		bReady &= Resource->GetStatus() == ERenderResourceStatus::Ready;
	}
	for (const auto& Material : Prepared.Materials)
	{
		if (Material->GetStatus() == ERenderMaterialStatus::Failed)
		{
			throw std::runtime_error(Material->GetError());
		}
		bReady &= Material->GetStatus() == ERenderMaterialStatus::Ready;
	}
	return bReady;
}
} // namespace Hyperion
