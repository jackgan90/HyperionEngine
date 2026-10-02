#include "PlacementService.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
std::string FormatPlacementPreparation(const FPlacementPreparation& InPreparation)
{
	if (InPreparation.State == EPlacementPreparationState::Ready)
	{
		return {};
	}
	if (InPreparation.State == EPlacementPreparationState::Failed && !InPreparation.Error.empty())
	{
		return InPreparation.Error;
	}
	if (InPreparation.Stage == EPlacementPreparationStage::Scene)
	{
		return "Wait for a valid scene document";
	}
	if (InPreparation.State == EPlacementPreparationState::Failed)
	{
		return "Required placement resource preparation failed";
	}
	switch (InPreparation.Stage)
	{
		case EPlacementPreparationStage::ModelLoading:
			return "Preparing model";
		case EPlacementPreparationStage::ModelUpload:
		case EPlacementPreparationStage::PreviewMaterial:
			return "Preparing model preview";
		case EPlacementPreparationStage::IconLoading:
		case EPlacementPreparationStage::IconUpload:
			return "Preparing icon";
		default:
			return "Preparing placement resources";
	}
}

FPlacementCandidate FPlacementService::Candidate(const FPlaceableObject& InObject)
{
	return {InObject.Id, InObject.Label, InObject.Create(), InObject.Model, InObject.Icon};
}

FPlacementCandidate FPlacementService::ModelCandidate(FAssetRef InReference, FAssetService& InAssets)
{
	if (InReference.TypeId != RecordType<FModelAsset>().Id)
	{
		throw std::invalid_argument("Only native model assets can be placed in the viewport");
	}
	InReference.Path = PathToUtf8(InAssets.Resolve(InReference, {}));
	std::string Key = "model:";
	for (const auto* Part : {&InReference.Id, &InReference.Path, &InReference.TypeId, &InReference.Revision})
	{
		Key += std::to_string(Part->size()) + ":" + *Part;
	}
	const auto Label = PathToUtf8(PathFromUtf8(InReference.Path).stem());
	return {std::move(Key), Label, {}, std::move(InReference), {}, true};
}

void FPlacementService::Prepare(const FPlacementCandidate& InCandidate, FSceneInstance& InScene)
{
	if (!InCandidate.Model || Models.contains(InCandidate.Id))
	{
		return;
	}
	FPlacementModel Model;
	Model.Asset = InScene.RegisterModelAsset(*InCandidate.Model);
	Models.emplace(InCandidate.Id, std::move(Model));
}

void FPlacementService::Poll(FSceneInstance& InScene, FRenderSession& InSession)
{
	const auto Assets = InScene.GetAssets();
	for (auto& [Id, Model] : Models)
	{
		for (const auto& Asset : Assets)
		{
			if (Asset.Id == Model.Asset)
			{
				Model.Error = Asset.Error;
				Model.bLoadComplete = Asset.bComplete;
				if (Model.Data != Asset.Data)
				{
					Model.Data = Asset.Data;
					Model.Resource.reset();
					Model.UploadError.reset();
					try
					{
						Model.Resource = Asset.Data ? InSession.GetResources().RequestModel(Asset.Data) : nullptr;
					}
					catch (const std::exception& Failure)
					{
						Model.UploadError = Failure.what();
					}
				}
			}
		}
	}
}

FPlacementPreparation FPlacementService::ModelPreparation(const FPlacementCandidate& InCandidate) const
{
	if (!InCandidate.Model)
	{
		return {};
	}
	const auto It = Models.find(InCandidate.Id);
	if (It == Models.end())
	{
		return {EPlacementPreparationState::Pending, EPlacementPreparationStage::ModelLoading};
	}
	const auto& Model = It->second;
	if (!Model.Error.empty() || (Model.bLoadComplete && !Model.Data))
	{
		return {EPlacementPreparationState::Failed, EPlacementPreparationStage::ModelLoading, Model.Error};
	}
	if (!Model.Data)
	{
		return {EPlacementPreparationState::Pending, EPlacementPreparationStage::ModelLoading};
	}
	if (Model.UploadError)
	{
		return {EPlacementPreparationState::Failed, EPlacementPreparationStage::ModelUpload, *Model.UploadError};
	}
	if (Model.Resource)
	{
		const auto State = Model.Resource->GetStatus();
		if (State == ERenderResourceStatus::Failed || State == ERenderResourceStatus::Retired)
		{
			return {EPlacementPreparationState::Failed, EPlacementPreparationStage::ModelUpload,
			        Model.Resource->GetError()};
		}
		if (State == ERenderResourceStatus::Ready)
		{
			return {};
		}
	}
	return {EPlacementPreparationState::Pending, EPlacementPreparationStage::ModelUpload};
}

FPlacementPreparation FPlacementService::ResolvePreparation(const FPlacementCandidate& InCandidate,
                                                            const FPlacementPreparation& InModel,
                                                            const FPlacementPreparationContext& InContext)
{
	if (!InContext.bSceneAvailable)
	{
		return {EPlacementPreparationState::Failed, EPlacementPreparationStage::Scene};
	}
	if (InCandidate.Model)
	{
		return InModel.State != EPlacementPreparationState::Ready ? InModel : InContext.PreviewMaterial;
	}
	if (!InCandidate.Icon.empty())
	{
		return InContext.Icon.value_or(FPlacementPreparation{
		    EPlacementPreparationState::Failed, EPlacementPreparationStage::IconLoading, "Unknown icon resource"});
	}
	return {};
}

FPlacementPreparation FPlacementService::GetPreparation(const FPlacementCandidate& InCandidate,
                                                        const FPlacementPreparationContext& InContext) const
{
	return ResolvePreparation(InCandidate, ModelPreparation(InCandidate), InContext);
}

FSceneHandle FPlacementService::Commit(const FPlacementCandidate& InCandidate, FVec3 InPosition,
                                       FSceneEditDocument& InDocument, const FPlacementPreparationContext& InContext)
{
	if (!IsFinite(InPosition) || GetPreparation(InCandidate, InContext).State != EPlacementPreparationState::Ready)
	{
		throw std::invalid_argument("Placement requires finite coordinates and prepared resources");
	}
	auto Node = InCandidate.Node;
	Node.Name = InCandidate.Label;
	Node.Local().Values[12] = InPosition.X;
	Node.Local().Values[13] = InPosition.Y;
	Node.Local().Values[14] = InPosition.Z;
	if (InCandidate.Model)
	{
		Node.Model() = FSceneModelComponent{};
		Node.Model()->Asset = Models.at(InCandidate.Id).Asset;
	}
	return InDocument.CommitCreate(std::move(Node));
}
} // namespace Hyperion
