#include "Hyperion/Renderer/Model.h"

namespace Hyperion
{
FModel::FModel(FRenderSceneClient& InScene, FRenderResourceService& InResources,
               std::shared_ptr<const FModelAsset> InAsset)
    : FModel(InScene, InResources, FSceneModel{"", PrepareSceneModel(std::move(InAsset))})
{
}

FModel::FModel(FRenderSceneClient& InScene, FRenderResourceService& InResources, const FSceneModel& InModel)
    : FModel(InScene, InResources, InModel, false)
{
}

FModel::FModel(FRenderSceneClient& InScene, FRenderResourceService& InResources, const FSceneModel& InModel,
               bool bInDeferred)
    : Scene(InScene), Resources(InResources)
{
	Scene.RequireMain();
	if (!InModel.Data || !InModel.Data->Asset)
	{
		throw std::invalid_argument("A render model requires prepared CPU geometry");
	}
	Data = InModel.Data;
	Asset = Data->Asset;
	Instances = Data->Instances;
	Resource = Resources.RequestModel(Asset);
	if (!bInDeferred)
	{
		FFrozenMaterials Frozen;
		auto Prepared = PrepareState(InModel, Frozen);
		std::vector<FRenderPrimitiveState> States;
		for (const auto& Update : Prepared.Updates)
		{
			States.push_back(Update.State);
		}
		Bindings = Scene.CreateBatch(std::move(States));
		CommitState(std::move(Prepared));
	}
}

FModel::~FModel()
{
	FRenderBinding::RemoveBatch(Bindings);
}

std::shared_ptr<const FMaterialSnapshot> FModel::FreezeSelection(const FSceneMaterialSelection& InSelection,
                                                                 FFrozenMaterials& InFrozen)
{
	if (!InSelection.Instance)
	{
		return InSelection.Snapshot;
	}
	auto& Snapshot = InFrozen[InSelection.Instance.get()];
	if (!Snapshot)
	{
		Snapshot = InSelection.Instance->Freeze();
	}
	return Snapshot;
}

FModel::FPreparedUpdate FModel::PrepareState(const FSceneModel& InModel, FFrozenMaterials& InFrozen) const
{
	Scene.RequireMain();
	if (InModel.Data != Data || !IsAffine(InModel.World))
	{
		throw std::invalid_argument("Model state requires the same prepared asset and an affine transform");
	}
	ValidateMaterialOverride(InModel.Material);
	ValidateSceneMaterialSelections(InModel);
	FPreparedUpdate Result{InModel, Revision + 1, {}};
	const auto ModelSnapshot = FreezeSelection(InModel.Surface, InFrozen);
	for (std::size_t Index = 0; Index < Instances.size(); ++Index)
	{
		FRenderPrimitiveState State;
		State.Resource = Resource;
		State.Section = Instances[Index].Primitive;
		State.World = Multiply(InModel.World, Instances[Index].World);
		State.Revision = Result.Revision;
		State.bVisible = InModel.bVisible;
		State.Material = InModel.Material;
		State.LocalBounds = Data->PrimitiveBounds[State.Section];
		State.ObjectParameters = InModel.Surface.Overrides;
		auto Snapshot = ModelSnapshot;
		const auto Selection = InModel.SectionSurfaces.find(State.Section);
		if (Selection != InModel.SectionSurfaces.end())
		{
			if (auto Selected = FreezeSelection(Selection->second, InFrozen))
			{
				Snapshot = std::move(Selected);
			}
			State.SectionParameters = Selection->second.Overrides;
		}
		if (Snapshot)
		{
			State.Surface = Resources.RequestMaterial(std::move(Snapshot));
		}
		ValidatePrimitiveState(State);
		Result.Updates.push_back(
		    {Bindings.empty() ? FRenderPrimitiveHandle{} : Bindings[Index].GetHandle(), std::move(State)});
	}
	return Result;
}

void FModel::CommitState(FPreparedUpdate InUpdate) noexcept
{
	Current = std::move(InUpdate.State);
	Revision = InUpdate.Revision;
}

FTaskHandle FModel::SetState(const FSceneModel& InModel)
{
	FFrozenMaterials Frozen;
	auto Prepared = PrepareState(InModel, Frozen);
	auto Receipt = Scene.Update(Prepared.Updates);
	if (!Receipt)
	{
		throw std::runtime_error("Model update was not admitted by its render scene");
	}
	CommitState(std::move(Prepared));
	return Receipt;
}

FTaskHandle FModel::SetTransform(FMat4 InWorld)
{
	Scene.RequireMain();
	auto State = Current;
	State.World = InWorld;
	return SetState(State);
}

FTaskHandle FModel::SetVisible(bool bInVisible)
{
	Scene.RequireMain();
	auto State = Current;
	State.bVisible = bInVisible;
	return SetState(State);
}

FTaskHandle FModel::SetMaterial(FMaterialOverride InMaterial)
{
	Scene.RequireMain();
	auto State = Current;
	State.Material = std::move(InMaterial);
	return SetState(State);
}

bool FModel::IsReady() const
{
	Scene.RequireMain();
	if (!Resource || Resource->GetStatus() != ERenderResourceStatus::Ready || Bindings.empty())
	{
		return false;
	}
	for (const auto& Binding : Bindings)
	{
		if (Binding.GetStatus().State != ERenderPrimitiveStatus::Ready)
		{
			return false;
		}
	}
	return true;
}

std::string FModel::GetError() const
{
	Scene.RequireMain();
	if (Resource && Resource->GetStatus() == ERenderResourceStatus::Failed)
	{
		return Resource->GetError();
	}
	for (const auto& Binding : Bindings)
	{
		const auto Status = Binding.GetStatus();
		if (!Status.Error.empty())
		{
			return Status.Error;
		}
	}
	return {};
}

std::vector<FRenderDrawResult> FModel::GetDrawResults() const
{
	Scene.RequireMain();
	std::vector<FRenderDrawResult> Results;
	for (const auto& Binding : Bindings)
	{
		Results.push_back(Binding.GetLastDrawResult());
	}
	return Results;
}

std::size_t FModel::PrimitiveCount() const
{
	Scene.RequireMain();
	return Bindings.size();
}

std::shared_ptr<const FRenderResource> FModel::GetResource() const
{
	Scene.RequireMain();
	return Resource;
}

void FModel::Remove()
{
	Scene.RequireMain();
	Scene.RemoveBatch(Bindings);
	Bindings.clear();
	Resource.reset();
}
} // namespace Hyperion
