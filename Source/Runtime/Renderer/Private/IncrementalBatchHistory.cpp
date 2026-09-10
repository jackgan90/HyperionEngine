#include "IncrementalBatchHistory.h"
#include "Hyperion/Core/Profiling.h"
#include "LocalMaterialPreparation.h"
#include "RenderBatchInternal.h"
#include "SceneItemPreparation.h"
#include <algorithm>
#include <limits>

namespace Hyperion
{
std::size_t FIncrementalSourceKeyHash::operator()(const FIncrementalSourceKey& InKey) const
{
	std::size_t Result{};
	for (const auto Word :
	     {std::get<0>(InKey), std::uint64_t(std::get<1>(InKey)), std::get<2>(InKey), std::get<3>(InKey)})
	{
		Result = Result * 16777619U ^ std::hash<std::uint64_t>{}(Word);
	}
	return Result;
}

FIncrementalSourceKey FIncrementalBatchHistory::Key(const FRenderItem& InItem)
{
	return {InItem.Primitive.Scene, InItem.Primitive.Slot, InItem.Primitive.Generation, *InItem.LocalItemId};
}

void FIncrementalBatchHistory::Remove(
    std::unordered_map<FIncrementalSourceKey, FSource, FIncrementalSourceKeyHash>::iterator InSource)
{
	if (InSource->second.Block < Blocks.size())
	{
		auto& Block = Blocks[InSource->second.Block];
		std::erase(Block.Members, InSource->first);
		Block.bDirty = true;
		Block.bRemap = true;
		if (Block.Members.empty())
		{
			Block.Contents.reset();
			Block.Chunk.reset();
		}
	}
	Sources.erase(InSource);
	bStructureChanged = true;
}

bool FIncrementalBatchHistory::UpdateMembership(const FRenderSceneSnapshot& InSnapshot, std::uint64_t InAccess,
                                                std::vector<std::size_t>& OutAdded, FRenderBatchStats& OutStats)
{
	HYP_PERF_SCOPE_C(Detail, UpdateIncrementalBatchMembers);
	bStructureChanged = !Structure;
	Access = InAccess;
	if (Structure && LocalContents.lock() == InSnapshot.LocalContentIdentity &&
	    Sources.size() == InSnapshot.Items.Size())
	{
		OutStats.IncrementalItemReuses = Sources.size();
		return true;
	}
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		if (!Item.LocalItemId || !Item.LocalPreparation || !Item.Preparation || !Item.ResolvedParameters ||
		    !Item.Lifetime || !Item.PreparationError.empty() || Item.Primitive.Scene != Scene ||
		    !Item.Primitive.Generation || Item.LocalPreparation->Preparation.lock() != Item.Preparation ||
		    Item.LocalPreparation->Parameters.lock() != Item.ResolvedParameters)
		{
			return false;
		}
		const auto SourceKey = Key(Item);
		auto It = Sources.find(SourceKey);
		if (It != Sources.end())
		{
			if (It->second.Access == Access)
			{
				return false; // Duplicate local identities cannot claim the same persistent member twice.
			}
			if (It->second.Preparation.lock() == Item.Preparation &&
			    It->second.Parameters.lock() == Item.ResolvedParameters)
			{
				if (It->second.Index != Index)
				{
					bStructureChanged = true;
					Blocks[It->second.Block].bRemap = true;
				}
				It->second.Index = Index;
				It->second.Access = Access;
				++OutStats.IncrementalItemReuses;
				continue;
			}
			Remove(It);
		}
		Sources.emplace(SourceKey, FSource{Item.Preparation, Item.ResolvedParameters,
		                                   std::numeric_limits<std::size_t>::max(), Index, Access});
		OutAdded.push_back(Index);
		bStructureChanged = true;
	}
	for (auto It = Sources.begin(); It != Sources.end();)
	{
		if (It->second.Access != Access)
		{
			Remove(It++);
		}
		else
		{
			++It;
		}
	}
	return true;
}

void FIncrementalBatchHistory::RefreshIndices()
{
	for (auto& Block : Blocks)
	{
		if (!Block.bRemap)
		{
			continue;
		}
		Block.Indices.clear();
		Block.Indices.reserve(Block.Members.size());
		for (const auto& SourceKey : Block.Members)
		{
			Block.Indices.push_back(Sources.at(SourceKey).Index);
		}
		Block.bRemap = false;
	}
}

bool FIncrementalBatchHistory::RefreshGroups(const FRenderSceneSnapshot& InSnapshot,
                                             std::vector<FRenderBatchSignature>& OutSignatures)
{
	HYP_PERF_SCOPE_C(Detail, RefreshIncrementalBatchGroups);
	std::vector<std::optional<std::size_t>> First(Groups.size());
	std::vector<const void*> Shared(Groups.size());
	std::vector<std::vector<std::shared_ptr<const FRenderBatchValues>>> Values(Groups.size());
	FSharedBatchValueCache SharedValues;
	for (const auto& Block : Blocks)
	{
		for (const auto Index : Block.Indices)
		{
			const auto& Item = InSnapshot.Items[Index];
			const void* Identity = Item.SharedParameters ? static_cast<const void*>(Item.SharedParameters.get())
			                                             : Item.ResolvedParameters->Values.GetSharedIdentity();
			if (First[Block.Group] && Shared[Block.Group] == Identity &&
			    Item.Preparation->Program == InSnapshot.Items[*First[Block.Group]].Preparation->Program)
			{
				continue;
			}
			const auto* Pass = Item.Preparation->Program->FindInstancePass(InSnapshot.View.Usage);
			if (!Pass)
			{
				return false;
			}
			const auto Current = SharedValues.Get(*Pass, Item);
			auto& Known = Values[Block.Group];
			if (First[Block.Group])
			{
				if (std::find(Known.begin(), Known.end(), Current) != Known.end())
				{
					continue;
				}
				if (Current->size() != Known.front()->size() ||
				    !std::equal(Current->begin(), Current->end(), Known.front()->begin(),
				                [](const auto& InA, const auto& InB)
				                {
					                return SameMaterialValue(InA, InB);
				                }))
				{
					return false; // Complete current shared values split this formerly compatible group.
				}
			}
			else
			{
				First[Block.Group] = Index;
				Shared[Block.Group] = Identity;
			}
			Known.push_back(Current);
		}
	}
	OutSignatures.resize(Groups.size());
	for (std::size_t Index = 0; Index < Groups.size(); ++Index)
	{
		auto& Group = Groups[Index];
		if (!First[Index])
		{
			Group.Structure.reset();
			continue;
		}
		OutSignatures[Index] = {Group.Structure, Values[Index].front()};
	}
	return true;
}

bool FIncrementalBatchHistory::Insert(const FRenderSceneSnapshot& InSnapshot, std::size_t InIndex,
                                      const FRenderBatchSignature& InSignature, std::uint32_t InCapacity,
                                      std::size_t InBlockLimit, std::vector<FRenderBatchSignature>& InSignatures)
{
	std::size_t GroupIndex{};
	while (GroupIndex < Groups.size() && (!Groups[GroupIndex].Structure || Groups[GroupIndex].Capacity != InCapacity ||
	                                      !(InSignatures[GroupIndex] == InSignature)))
	{
		++GroupIndex;
	}
	if (GroupIndex == Groups.size())
	{
		const auto Empty = std::find_if(Groups.begin(), Groups.end(),
		                                [](const auto& InGroup)
		                                {
			                                return !InGroup.Structure;
		                                });
		GroupIndex = std::distance(Groups.begin(), Empty);
		if (Empty != Groups.end())
		{
			*Empty = {InSignature.Structure, InCapacity};
			InSignatures[GroupIndex] = InSignature;
		}
		else
		{
			if (Groups.size() >= InBlockLimit)
			{
				return false;
			}
			Groups.push_back({InSignature.Structure, InCapacity});
			InSignatures.push_back(InSignature);
		}
	}
	auto Block = std::find_if(Blocks.begin(), Blocks.end(),
	                          [&](const auto& InBlock)
	                          {
		                          return !InBlock.Members.empty() && InBlock.Group == GroupIndex &&
		                                 InBlock.Members.size() < InCapacity;
	                          });
	if (Block == Blocks.end())
	{
		Block = std::find_if(Blocks.begin(), Blocks.end(),
		                     [](const auto& InBlock)
		                     {
			                     return InBlock.Members.empty();
		                     });
		if (Block == Blocks.end())
		{
			if (Blocks.size() >= InBlockLimit)
			{
				return false;
			}
			Blocks.push_back({});
			Block = std::prev(Blocks.end());
		}
		Block->Group = GroupIndex;
	}
	const auto SourceKey = Key(InSnapshot.Items[InIndex]);
	Sources.at(SourceKey).Block = std::distance(Blocks.begin(), Block);
	Block->Members.push_back(SourceKey);
	Block->bDirty = true;
	Block->bRemap = true;
	return true;
}
} // namespace Hyperion
