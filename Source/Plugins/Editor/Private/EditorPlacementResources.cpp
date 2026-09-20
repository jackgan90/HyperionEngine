#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
std::string FEditorPlugin::PlacementUnavailableReason(const FPlaceableObject& InObject) const
{
	if (!Scene->GetStatus().bLoaded || !Scene->GetStatus().Error.empty())
	{
		return "Wait for a valid scene document";
	}
	if (InObject.Model)
	{
		const auto It = PlacementModels.find(InObject.Id);
		if (It == PlacementModels.end())
		{
			return "Preparing model";
		}
		if (!It->second.Error.empty())
		{
			return It->second.Error;
		}
		if (PlacementMaterial->GetStatus() == ERenderMaterialStatus::Failed)
		{
			return PlacementMaterial->GetError();
		}
		return It->second.Resource && It->second.Resource->GetStatus() == ERenderResourceStatus::Ready &&
		               PlacementMaterial->GetStatus() == ERenderMaterialStatus::Ready
		           ? ""
		           : "Preparing model preview";
	}
	if (const auto It = PlacementIcons.find(InObject.Icon); It != PlacementIcons.end())
	{
		return !It->second.Error.empty() ? It->second.Error : It->second.Source.Texture ? "" : "Preparing icon";
	}
	return InObject.Icon.empty() ? "" : "Unknown icon resource";
}

void FEditorPlugin::InitializePlacement()
{
	for (const auto* Category : {"Basic", "Shapes", "Lights"})
	{
		PlacementRegistry.AddCategory(Category);
	}
	for (const std::string Name : {"Cube", "Sphere", "Cylinder", "Cone", "Plane"})
	{
		FAssetRef Reference;
		Reference.TypeId = RecordType<FModelAsset>().Id;
		Reference.Path = "/Engine/Models/Primitives/" + Name + ".hasset";
		PlacementRegistry.Add({Name,
		                       Name,
		                       {"Basic", "Shapes"},
		                       Reference,
		                       {},
		                       []
		                       {
			                       return FSceneNode{};
		                       }});
	}
	PlacementRegistry.Add({"DirectionalLight",
	                       "Directional Light",
	                       {"Basic", "Lights"},
	                       {},
	                       "DirectionalLight",
	                       []
	                       {
		                       return MakeSceneDirectionalLightNode({});
	                       }});
	PlacementRegistry.Add({"PointLight",
	                       "Point Light",
	                       {"Basic", "Lights"},
	                       {},
	                       "PointLight",
	                       []
	                       {
		                       FSceneNode Node;
		                       Node.PointLight() = FScenePointLight{};
		                       return Node;
	                       }});
	PlacementRegistry.Add({"SpotLight",
	                       "Spot Light",
	                       {"Lights"},
	                       {},
	                       "SpotLight",
	                       []
	                       {
		                       FSceneNode Node;
		                       Node.SpotLight() = FSceneSpotLight{};
		                       Node.Local() = SceneCameraTransform({}, {0, -1, -.2f});
		                       return Node;
	                       }});
	PlacementLifetime = Session->GetResources().CreateScopeLifetime();
	PlacementMaterial = Session->GetResources().RequestMaterial(MakeGeometryPreviewMaterial());
	std::uint64_t Texture = 10;
	for (const std::string Name : {"DirectionalLight", "PointLight", "SpotLight"})
	{
		auto& Icon = PlacementIcons[Name];
		Icon.Texture = Texture++;
		try
		{
			Icon.Request = Assets.LoadAsync<FTextureAsset>("/Engine/Editor/Icons/" + Name + ".hasset");
		}
		catch (const std::exception& Failure)
		{
			Icon.Error = Failure.what();
			Icon.bComplete = true;
		}
	}
}

void FEditorPlugin::PollPlacementResources()
{
	if (PlacementPublication)
	{
		const auto* Node = Scene->FindNode(*PlacementPublication);
		const auto Results = Scene->GetDrawResults(*PlacementPublication);
		if (!Node || !Results.empty() || Node->Local().Values != PlacementPublicationLocal.Values)
		{
			PlacementPublication.reset();
			PlacementPublicationPreview.reset();
		}
	}
	PollPlacementIcons();
	if (!bShowPlacement || !Scene->GetStatus().bLoaded || !Scene->GetStatus().Error.empty())
	{
		return;
	}
	for (const auto* Object : PlacementRegistry.Search("All", {}))
	{
		if (Object->Model && !PlacementModels.contains(Object->Id))
		{
			auto& Model = PlacementModels[Object->Id];
			try
			{
				Model.Asset = Scene->RegisterModelAsset(*Object->Model);
			}
			catch (const std::exception& Failure)
			{
				Model.Error = Failure.what();
			}
		}
	}
	const auto AssetsInScene = Scene->GetAssets();
	for (auto& [Id, Model] : PlacementModels)
	{
		for (const auto& Asset : AssetsInScene)
		{
			if (Asset.Id == Model.Asset)
			{
				Model.Error = Asset.Error;
				if (!Model.Resource && Asset.Data)
				{
					Model.Data = Asset.Data;
					Model.Resource = Session->GetResources().RequestModel(Asset.Data);
				}
			}
		}
		if (Model.Resource && Model.Resource->GetStatus() == ERenderResourceStatus::Failed)
		{
			Model.Error = Model.Resource->GetError();
		}
	}
}

void FEditorPlugin::PollPlacementIcons()
{
	for (auto& [Id, Icon] : PlacementIcons)
	{
		if (!Icon.bComplete && Icon.Request.Ready())
		{
			Icon.bComplete = true;
			try
			{
				Icon.PendingSource = {ERenderTargetKind::Texture,
				                      std::make_shared<const FMaterialTextureSource>(Icon.Request.GetReady()),
				                      PlacementLifetime};
			}
			catch (const std::exception& Failure)
			{
				Icon.Error = Failure.what();
			}
		}
		if (Icon.PendingSource.Texture && Icon.Error.empty())
		{
			try
			{
				bool bReady{};
				const auto Endpoint = Session->GetResources().GetPreparation();
				const auto Source = Icon.PendingSource;
				Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
				                          [&]
				                          {
					                          const std::array Sources{Source.Texture};
					                          bReady = Endpoint.PrepareTextures(Sources, Source.Lifetime);
				                          }));
				if (bReady)
				{
					Icon.Source = std::exchange(Icon.PendingSource, {});
				}
			}
			catch (const std::exception& Failure)
			{
				Icon.Error = Failure.what();
			}
		}
	}
}

std::shared_ptr<const FTransientGeometry> FEditorPlugin::FreezePlacementPreview() const
{
	if (!Placement.GetPreview())
	{
		return PlacementPublication && Scene->FindNode(*PlacementPublication) ? PlacementPublicationPreview : nullptr;
	}
	const auto It = PlacementModels.find(Placement.GetType());
	if (It == PlacementModels.end() || !It->second.Resource)
	{
		return {};
	}
	const auto& Model = It->second;
	auto Result = std::make_shared<FTransientGeometry>();
	Result->Lifetime = PlacementLifetime;
	for (const auto& Instance : Model.Data->Instances)
	{
		FRenderPrimitiveState State;
		State.Resource = Model.Resource;
		State.Section = Instance.Primitive;
		State.Surface = PlacementMaterial;
		State.World = Multiply(Translation(Placement.GetPreview()->Position), Instance.World);
		Result->Items.push_back(std::move(State));
	}
	return Result;
}
} // namespace Hyperion
