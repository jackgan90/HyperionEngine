#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
FPlacementPreparation FEditorPlugin::GetPlacementPreparation(const FPlaceableObject& InObject) const
{
	return GetPlacementPreparation(FPlacementService::Candidate(InObject));
}

FPlacementPreparation FEditorPlugin::GetPlacementPreparation(const FPlacementCandidate& InObject) const
{
	return PlacementService.GetPreparation(InObject, GetPlacementPreparationContext(InObject));
}

FPlacementPreparationContext FEditorPlugin::GetPlacementPreparationContext(const FPlacementCandidate& InObject) const
{
	FPlacementPreparationContext Result;
	const auto& Status = Scene->GetStatus();
	Result.bSceneAvailable = Status.bLoaded && Status.Error.empty();
	if (InObject.Model)
	{
		const auto State = PlacementMaterial ? PlacementMaterial->GetStatus() : ERenderMaterialStatus::Preparing;
		const bool bFailed = State == ERenderMaterialStatus::Failed || State == ERenderMaterialStatus::Retired;
		Result.PreviewMaterial = {State == ERenderMaterialStatus::Ready ? EPlacementPreparationState::Ready
		                          : bFailed                             ? EPlacementPreparationState::Failed
		                                                                : EPlacementPreparationState::Pending,
		                          EPlacementPreparationStage::PreviewMaterial,
		                          bFailed ? PlacementMaterial->GetError() : std::string{}};
	}
	if (const auto It = PlacementIcons.find(InObject.Icon); It != PlacementIcons.end())
	{
		Result.Icon = It->second.Preparation;
	}
	return Result;
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
			Icon.Preparation = {EPlacementPreparationState::Failed, EPlacementPreparationStage::IconLoading,
			                    Failure.what()};
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
				PlacementModels[Object->Id].bLoadComplete = true;
			}
		}
	}
	PlacementService.Poll(*Scene, *Session);
}

void FEditorPlugin::PollPlacementIcons()
{
	for (auto& [Id, Icon] : PlacementIcons)
	{
		if (Icon.Preparation.State == EPlacementPreparationState::Pending &&
		    Icon.Preparation.Stage == EPlacementPreparationStage::IconLoading && Icon.Request.Ready())
		{
			try
			{
				Icon.PendingSource = {ERenderTargetKind::Texture,
				                      std::make_shared<const FMaterialTextureSource>(Icon.Request.GetReady()),
				                      PlacementLifetime};
				Icon.Preparation = {EPlacementPreparationState::Pending, EPlacementPreparationStage::IconUpload};
			}
			catch (const std::exception& Failure)
			{
				Icon.Preparation = {EPlacementPreparationState::Failed, EPlacementPreparationStage::IconLoading,
				                    Failure.what()};
			}
		}
		if (Icon.PendingSource.Texture && Icon.Preparation.State == EPlacementPreparationState::Pending)
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
					Icon.Preparation = {};
				}
			}
			catch (const std::exception& Failure)
			{
				Icon.Preparation = {EPlacementPreparationState::Failed, EPlacementPreparationStage::IconUpload,
				                    Failure.what()};
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
