#include "PlacementService.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
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
				if (Model.Data != Asset.Data)
				{
					Model.Data = Asset.Data;
					Model.Resource = Asset.Data ? InSession.GetResources().RequestModel(Asset.Data) : nullptr;
				}
			}
		}
		if (Model.Resource && Model.Resource->GetStatus() == ERenderResourceStatus::Failed)
		{
			Model.Error = Model.Resource->GetError();
		}
	}
}

std::string FPlacementService::Unavailable(const FPlacementCandidate& InCandidate) const
{
	if (!InCandidate.Model)
	{
		return {};
	}
	const auto It = Models.find(InCandidate.Id);
	if (It == Models.end())
	{
		return "Preparing model";
	}
	const auto& Model = It->second;
	if (!Model.Error.empty())
	{
		return Model.Error;
	}
	return Model.Resource && Model.Resource->GetStatus() == ERenderResourceStatus::Ready ? ""
	                                                                                     : "Preparing model preview";
}

FSceneHandle FPlacementService::Commit(const FPlacementCandidate& InCandidate, FVec3 InPosition,
                                       FSceneEditDocument& InDocument)
{
	if (!IsFinite(InPosition) || !Unavailable(InCandidate).empty())
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
