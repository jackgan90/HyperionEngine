#include "Hyperion/Scene/Scene.h"
#include <algorithm>
#include <atomic>

namespace Hyperion
{
namespace
{
void ValidateSceneModel(const FSceneModel& InModel)
{
	if (!IsAffine(InModel.World))
	{
		throw std::invalid_argument("Scene transform must be finite and affine");
	}
	ValidateMaterialOverride(InModel.Material);
}
} // namespace

FScene::FScene() : Owner(std::this_thread::get_id())
{
	static std::atomic_uint64_t NextIdentity{1};
	Identity = NextIdentity.fetch_add(1);
}

void FScene::RequireMain() const
{
	if (std::this_thread::get_id() != Owner)
	{
		throw std::logic_error("Logical scene accessed outside its Main owner");
	}
}

std::uint64_t FScene::GetIdentity() const
{
	RequireMain();
	return Identity;
}

FSceneHandle FScene::Add(FSceneModel InModel)
{
	RequireMain();
	ValidateSceneModel(InModel);
	const auto It = std::find_if(Slots.begin(), Slots.end(),
	                             [](const FSlot& InSlot)
	                             {
		                             return !InSlot.Model;
	                             });
	const auto Index = static_cast<std::uint32_t>(It - Slots.begin());
	if (Index == Slots.size())
	{
		Slots.push_back({});
	}
	auto& Slot = Slots[Index];
	const FSceneHandle Handle{Identity, Index, Slot.Generation + 1};
	Changes.insert_or_assign(Handle, FSceneChange{Handle, Revision + 1, InModel});
	Slot.Model = std::move(InModel);
	++Slot.Generation;
	++Revision;
	return Handle;
}

const FSceneModel* FScene::Find(FSceneHandle InHandle) const
{
	RequireMain();
	if (InHandle.Scene != Identity || InHandle.Slot >= Slots.size())
	{
		return nullptr;
	}
	const auto& Slot = Slots[InHandle.Slot];
	return Slot.Generation == InHandle.Generation && Slot.Model ? &*Slot.Model : nullptr;
}

bool FScene::Update(FSceneHandle InHandle, FSceneModel InModel)
{
	if (!Find(InHandle))
	{
		return false;
	}
	ValidateSceneModel(InModel);
	Changes.insert_or_assign(InHandle, FSceneChange{InHandle, Revision + 1, InModel});
	Slots[InHandle.Slot].Model = std::move(InModel);
	++Revision;
	return true;
}

bool FScene::Remove(FSceneHandle InHandle)
{
	if (!Find(InHandle))
	{
		return false;
	}
	Changes.insert_or_assign(InHandle, FSceneChange{InHandle, Revision + 1, {}});
	Slots[InHandle.Slot].Model.reset();
	++Revision;
	return true;
}

std::vector<FSceneHandle> FScene::GetHandles() const
{
	RequireMain();
	std::vector<FSceneHandle> Handles;
	for (std::uint32_t Index = 0; Index < Slots.size(); ++Index)
	{
		if (Slots[Index].Model)
		{
			Handles.push_back({Identity, Index, Slots[Index].Generation});
		}
	}
	return Handles;
}

std::vector<FSceneChange> FScene::GetChanges() const
{
	RequireMain();
	std::vector<FSceneChange> Result;
	for (const auto& [Handle, Change] : Changes)
	{
		Result.push_back(Change);
	}
	std::sort(Result.begin(), Result.end(),
	          [](const FSceneChange& InA, const FSceneChange& InB)
	          {
		          return InA.Revision < InB.Revision;
	          });
	return Result;
}

void FScene::Acknowledge(std::uint64_t InRevision)
{
	RequireMain();
	std::erase_if(Changes,
	              [InRevision](const auto& InPair)
	              {
		              return InPair.second.Revision <= InRevision;
	              });
}

void FScene::Clear()
{
	for (const auto Handle : GetHandles())
	{
		Remove(Handle);
	}
}

void FScene::BeginSynchronization()
{
	RequireMain();
	if (bSynchronizing)
	{
		throw std::logic_error("Logical scene already has a synchronization consumer");
	}
	for (const auto Handle : GetHandles())
	{
		Update(Handle, *Find(Handle));
	}
	bSynchronizing = true;
}

void FScene::EndSynchronization()
{
	RequireMain();
	bSynchronizing = false;
}
} // namespace Hyperion
