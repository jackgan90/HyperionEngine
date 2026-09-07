#include "Hyperion/Renderer/Model.h"

namespace Hyperion
{
FModel::FModel(FRenderSceneClient& InScene, FRenderResourceService& InResources,
               std::shared_ptr<const FModelAsset> InAsset)
    : Scene(InScene), Asset(std::move(InAsset)), Resource(InResources.RequestModel(Asset)),
      Instances(ModelInstances(*Asset))
{
	std::vector<FRenderPrimitiveState> States;
	States.reserve(Instances.size());
	for (const auto& Instance : Instances)
	{
		FRenderPrimitiveState State;
		State.Resource = Resource;
		State.Section = Instance.Primitive;
		State.World = Instance.World;
		States.push_back(std::move(State));
	}
	Bindings = Scene.CreateBatch(std::move(States));
}

FTaskHandle FModel::Publish()
{
	Scene.RequireMain();
	std::vector<FRenderPrimitiveUpdate> Updates;
	++Revision;
	for (std::size_t Index = 0; Index < Bindings.size(); ++Index)
	{
		FRenderPrimitiveState State;
		State.Resource = Resource;
		State.Section = Instances[Index].Primitive;
		State.World = Multiply(World, Instances[Index].World);
		State.Revision = Revision;
		State.bVisible = bVisible;
		State.Material = Material;
		Updates.push_back({Bindings[Index].GetHandle(), std::move(State)});
	}
	return Scene.Update(std::move(Updates));
}

FTaskHandle FModel::SetTransform(FMat4 InWorld)
{
	Scene.RequireMain();
	World = InWorld;
	return Publish();
}

FTaskHandle FModel::SetVisible(bool bInVisible)
{
	Scene.RequireMain();
	bVisible = bInVisible;
	return Publish();
}

FTaskHandle FModel::SetMaterial(FMaterialOverride InMaterial)
{
	Scene.RequireMain();
	Material = std::move(InMaterial);
	return Publish();
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
	Bindings.clear();
	Resource.reset();
}
} // namespace Hyperion
