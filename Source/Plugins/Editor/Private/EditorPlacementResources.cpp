#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
std::string FEditorPlugin::PlacementUnavailableReason(const FPlaceableObject& InObject) const
{
	return PlacementUnavailableReason(FPlacementService::Candidate(InObject));
}

std::string FEditorPlugin::PlacementUnavailableReason(const FPlacementCandidate& InObject) const
{
	if (!Scene->GetStatus().bLoaded || !Scene->GetStatus().Error.empty())
	{
		return "Wait for a valid scene document";
	}
	if (InObject.Model)
	{
		const auto Unavailable = PlacementService.Unavailable(InObject);
		if (!Unavailable.empty())
		{
			return Unavailable;
		}
		if (PlacementMaterial->GetStatus() == ERenderMaterialStatus::Failed)
		{
			return PlacementMaterial->GetError();
		}
		return PlacementMaterial->GetStatus() == ERenderMaterialStatus::Ready ? "" : "Preparing model preview";
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
	PlacementRegistry.Add({"SkyLight",
	                       "Sky Light",
	                       {"Basic", "Lights"},
	                       {},
	                       "SkyLight",
	                       []
	                       {
		                       FSceneNode Node;
		                       Node.EnvironmentLight() = FSceneEnvironmentLight{};
		                       return Node;
	                       }});
	PlacementLifetime = Session->GetResources().CreateScopeLifetime();
	PlacementMaterial = Session->GetResources().RequestMaterial(MakeGeometryPreviewMaterial());
	std::uint64_t Texture = 10;
	for (const std::string Name : {"DirectionalLight", "PointLight", "SpotLight", "SkyLight"})
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
		if (!Node || !Scene->GetStatus().Error.empty() || !Scene->GetStatus().PublicationError.empty() ||
		    Scene->IsModelReady(*PlacementPublication) || !Scene->GetError(*PlacementPublication).empty() ||
		    Scene->GetRevision() != PlacementPublicationRevision ||
		    Node->Local().Values != PlacementPublicationLocal.Values)
		{
			PlacementPublication.reset();
			PlacementPublicationPreview.reset();
		}
	}
	PollPlacementIcons();
	if (!Scene->GetStatus().bLoaded || !Scene->GetStatus().Error.empty())
	{
		return;
	}
	for (const auto* Object : PlacementRegistry.Search("All", {}))
	{
		if (bShowPlacement && Object->Model && !PlacementModels.contains(Object->Id))
		{
			try
			{
				PlacementService.Prepare(FPlacementService::Candidate(*Object), *Scene);
			}
			catch (const std::exception& Failure)
			{
				PlacementModels[Object->Id].Error = Failure.what();
			}
		}
	}
	PlacementService.Poll(*Scene, *Session);
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
		if (!PlacementPublication || !PlacementPublicationPreview || !Scene->GetStatus().Error.empty() ||
		    !Scene->GetStatus().PublicationError.empty() || !Scene->FindNode(*PlacementPublication) ||
		    Scene->IsModelReady(*PlacementPublication) || !Scene->GetError(*PlacementPublication).empty() ||
		    Scene->GetRevision() != PlacementPublicationRevision)
		{
			return {};
		}
		auto Result = std::make_shared<FTransientGeometry>();
		Result->Lifetime = PlacementPublicationPreview->Lifetime;
		Result->ReplacedPrimitives = Scene->ResolveRenderPrimitives(*PlacementPublication);
		for (const auto* Items : {&PlacementPublicationPreview->Items, &PlacementPublicationPreview->SceneItems})
		{
			for (const auto& State : *Items)
			{
				if (bPlacementPublicationSourceMaterials)
				{
					Result->AddModelInstance(State, PlacementMaterial);
				}
				else
				{
					Result->Items.push_back(State);
				}
			}
		}
		return Result;
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
		if (PlacementCandidate && PlacementCandidate->bPreferModelMaterials)
		{
			Result->AddModelInstance(std::move(State), PlacementMaterial);
		}
		else
		{
			Result->Items.push_back(std::move(State));
		}
	}
	return Result;
}
} // namespace Hyperion
