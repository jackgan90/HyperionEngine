#include "InstanceDataCache.h"
#include "Hyperion/Renderer/MaterialInputValues.h"
#include "Hyperion/Renderer/MaterialPacking.h"
#include "SceneItemPreparation.h"
#include <algorithm>
#include <list>
#include <map>
#include <tuple>
#include <unordered_map>

namespace Hyperion
{
namespace
{
struct FRecordLayout
{
	std::uint64_t Identity{};
	EShaderFormat Format{};
	std::uint32_t Stride{};
	std::vector<FShaderMember> Members;
	std::vector<std::pair<std::string, std::string>> Mapping;

	bool Matches(const FRecordLayout& InOther) const
	{
		return Format == InOther.Format && Stride == InOther.Stride && Members == InOther.Members &&
		       Mapping == InOther.Mapping;
	}
};

struct FPackedRecord
{
	std::uint64_t Identity{};
	std::shared_ptr<const std::vector<std::byte>> Data;
};

using FRecordKey = std::tuple<std::uint64_t, std::uint64_t, std::uint32_t, std::uint64_t, std::uint64_t>;
using FBlockKey = std::pair<std::uint64_t, std::vector<std::uint64_t>>;

struct FRecordKeyHash
{
	std::size_t operator()(const FRecordKey& InKey) const
	{
		std::size_t Hash{};
		for (const auto Word : {std::get<0>(InKey), std::get<1>(InKey), std::uint64_t(std::get<2>(InKey)),
		                        std::get<3>(InKey), std::get<4>(InKey)})
		{
			Hash = Hash * 16777619U ^ std::hash<std::uint64_t>{}(Word);
		}
		return Hash;
	}
};

struct FBlockKeyHash
{
	std::size_t operator()(const FBlockKey& InKey) const
	{
		std::size_t Hash = std::hash<std::uint64_t>{}(InKey.first);
		for (std::size_t Index = 0; Index < InKey.second.size(); ++Index)
		{
			Hash = Hash * 16777619U ^ std::hash<std::uint64_t>{}(InKey.second[Index]);
		}
		return Hash;
	}
};

bool SameValues(const std::vector<std::shared_ptr<const FMaterialValue>>& InA,
                const std::vector<std::shared_ptr<const FMaterialValue>>& InB)
{
	return InA.size() == InB.size() && std::equal(InA.begin(), InA.end(), InB.begin(), SameMaterialValue);
}
} // namespace

struct FInstanceDataCache::FImpl
{
	struct FProgramLayout
	{
		std::weak_ptr<const FCompiledMaterialDefinition> Program;
		std::shared_ptr<const FRecordLayout> Layout;
	};

	struct FRecord
	{
		FPackedRecord Packed;
		std::vector<std::shared_ptr<const FMaterialValue>> Values;
		std::weak_ptr<const void> Owner;
		std::size_t Bytes{};
		std::list<FRecordKey>::iterator Recent;
		std::array<std::weak_ptr<const FLocalMaterialItem>, 8> LocalPreparations;

		bool MatchesLocal(const FRenderItem& InItem) const
		{
			if (!InItem.LocalPreparation)
			{
				return false;
			}
			for (std::size_t Index = 0; Index < LocalPreparations.size(); ++Index)
			{
				const auto& Proof = LocalPreparations[Index];
				if (!Proof.owner_before(InItem.LocalPreparation) && !InItem.LocalPreparation.owner_before(Proof))
				{
					return true;
				}
			}
			return false;
		}

		void RetainLocal(const FRenderItem& InItem)
		{
			if (!InItem.LocalPreparation)
			{
				return;
			}
			for (std::size_t Index = 0; Index < LocalPreparations.size(); ++Index)
			{
				if (LocalPreparations[Index].expired())
				{
					LocalPreparations[Index] = InItem.LocalPreparation;
					return;
				}
			}
		}
	};

	struct FBlock
	{
		std::shared_ptr<const std::vector<std::byte>> Data;
		std::vector<std::weak_ptr<const void>> Owners;
		std::size_t Bytes{};
		std::list<FBlockKey>::iterator Recent;
	};

	FRenderBatchLimits Limits;
	std::uint64_t NextIdentity{};
	std::map<std::pair<const FCompiledMaterialDefinition*, const FMaterialProgramBinding*>, FProgramLayout> Layouts;
	using FRecords = std::unordered_map<FRecordKey, FRecord, FRecordKeyHash>;
	FRecords Records;
	std::list<FRecordKey> RecentRecords;
	using FBlocks = std::unordered_map<FBlockKey, FBlock, FBlockKeyHash>;
	FBlocks Blocks;
	std::list<FBlockKey> RecentBlocks;
	std::size_t RecordBytes{};
	std::size_t BlockBytes{};

	explicit FImpl(FRenderBatchLimits InLimits) : Limits(InLimits)
	{
	}

	std::shared_ptr<const FRecordLayout> Layout(std::shared_ptr<const FCompiledMaterialDefinition> InProgram,
	                                            const FCompiledMaterialPass& InPass, std::uint32_t InSlot)
	{
		const auto& Binding = InPass.Bindings.at(InSlot);
		const auto Key = std::pair{InProgram.get(), &Binding};
		if (const auto It = Layouts.find(Key); It != Layouts.end() && It->second.Program.lock() == InProgram)
		{
			return It->second.Layout;
		}
		FRecordLayout Candidate;
		Candidate.Format = InPass.Vertex.Format;
		Candidate.Stride = Binding.InstanceStride;
		for (const auto& Member : Binding.Members)
		{
			Candidate.Members.push_back(Member.Layout);
			const auto& Parameter = InProgram->Interface.Schema->GetParameters().at(Member.ParameterIndex);
			Candidate.Mapping.emplace_back(Parameter.Name, Parameter.Semantic);
		}
		std::shared_ptr<const FRecordLayout> Result;
		for (const auto& [ExistingKey, Existing] : Layouts)
		{
			if (Existing.Layout->Matches(Candidate))
			{
				Result = Existing.Layout;
				break;
			}
		}
		if (!Result)
		{
			Candidate.Identity = ++NextIdentity;
			Result = std::make_shared<const FRecordLayout>(std::move(Candidate));
		}
		if (Layouts.size() >= 256)
		{
			Layouts.erase(Layouts.begin());
		}
		Layouts.insert_or_assign(Key, FProgramLayout{InProgram, Result});
		return Result;
	}

	void EraseRecord(FRecords::iterator InEntry)
	{
		RecordBytes -= InEntry->second.Bytes;
		RecentRecords.erase(InEntry->second.Recent);
		Records.erase(InEntry);
	}

	void EraseBlock(FBlocks::iterator InEntry)
	{
		BlockBytes -= InEntry->second.Bytes;
		RecentBlocks.erase(InEntry->second.Recent);
		Blocks.erase(InEntry);
	}

	FPackedRecord Record(const FRenderItem& InItem, const FMaterialProgramBinding& InBinding,
	                     const FRecordLayout& InLayout, FRenderBatchStats& OutStats)
	{
		const FRecordKey Key{InLayout.Identity, InItem.Primitive.Scene, InItem.Primitive.Slot,
		                     InItem.Primitive.Generation, InItem.LocalItemId.value_or(InItem.Ordinal)};
		const bool bStable = InItem.LocalItemId.has_value() && bool(InItem.Lifetime);
		const auto Existing = Records.find(Key);
		if (bStable && Existing != Records.end() && Existing->second.MatchesLocal(InItem))
		{
			RecentRecords.splice(RecentRecords.end(), RecentRecords, Existing->second.Recent);
			++OutStats.ReusedRecords;
			++OutStats.LocalRecordReuses;
			return Existing->second.Packed;
		}
		std::vector<std::shared_ptr<const FMaterialValue>> Values;
		for (const auto& Member : InBinding.Members)
		{
			Values.push_back(InItem.GetMaterialValue(Member.ParameterIndex));
		}
		if (bStable && Existing != Records.end() && SameValues(Existing->second.Values, Values))
		{
			Existing->second.Owner = InItem.Lifetime;
			Existing->second.RetainLocal(InItem);
			RecentRecords.splice(RecentRecords.end(), RecentRecords, Existing->second.Recent);
			++OutStats.ReusedRecords;
			return Existing->second.Packed;
		}
		const auto Parameters = InItem.SharedParameters
		                            ? ComposeMaterialParameters(*InItem.ResolvedParameters, *InItem.SharedParameters)
		                            : *InItem.ResolvedParameters;
		FPackedRecord Result{++NextIdentity, std::make_shared<const std::vector<std::byte>>(
		                                         PackMaterialConstants(InBinding, Parameters.Values))};
		++OutStats.PackedRecords;
		OutStats.PackedBytes += Result.Data->size();
		std::size_t Bytes = sizeof(FRecord) + Result.Data->size() + Values.capacity() * sizeof(Values.front());
		for (const auto& Value : Values)
		{
			Bytes += Value ? MaterialValueStorageBytes(*Value) : 0;
		}
		if (Existing != Records.end())
		{
			EraseRecord(Existing);
		}
		if (bStable && Limits.MaxItems && Bytes <= Limits.MaxBytes / 2)
		{
			while (!RecentRecords.empty() &&
			       (Records.size() >= Limits.MaxItems || RecordBytes + Bytes > Limits.MaxBytes / 2))
			{
				EraseRecord(Records.find(RecentRecords.front()));
				++OutStats.Evictions;
			}
			RecentRecords.push_back(Key);
			FRecord Entry{Result, std::move(Values), InItem.Lifetime, Bytes, std::prev(RecentRecords.end())};
			Entry.RetainLocal(InItem);
			Records.emplace(Key, std::move(Entry));
			RecordBytes += Bytes;
		}
		return Result;
	}

	std::shared_ptr<const std::vector<std::byte>> Block(const FRecordLayout& InLayout,
	                                                    const std::vector<FPackedRecord>& InRecords,
	                                                    const std::vector<std::weak_ptr<const void>>& InOwners,
	                                                    bool bInStable, FRenderBatchStats& OutStats)
	{
		FBlockKey Key{InLayout.Identity, {}};
		Key.second.reserve(InRecords.size());
		for (const auto& Record : InRecords)
		{
			Key.second.push_back(Record.Identity);
		}
		if (const auto It = Blocks.find(Key); bInStable && It != Blocks.end())
		{
			It->second.Owners = InOwners;
			RecentBlocks.splice(RecentBlocks.end(), RecentBlocks, It->second.Recent);
			++OutStats.ReusedBlocks;
			return It->second.Data;
		}
		auto Result = std::make_shared<std::vector<std::byte>>();
		Result->reserve(InRecords.size() * InLayout.Stride);
		for (const auto& Record : InRecords)
		{
			Result->insert(Result->end(), Record.Data->begin(), Record.Data->end());
		}
		++OutStats.AssembledBlocks;
		OutStats.AssembledBytes += Result->size();
		const auto Bytes = sizeof(FBlock) + Result->size() + Key.second.capacity() * sizeof(std::uint64_t) * 2 +
		                   InOwners.size() * sizeof(std::weak_ptr<const void>);
		if (bInStable && Limits.MaxChunks && Bytes <= Limits.MaxBytes / 2)
		{
			while (!RecentBlocks.empty() &&
			       (Blocks.size() >= Limits.MaxChunks || BlockBytes + Bytes > Limits.MaxBytes / 2))
			{
				EraseBlock(Blocks.find(RecentBlocks.front()));
				++OutStats.Evictions;
			}
			RecentBlocks.push_back(Key);
			Blocks.emplace(Key, FBlock{Result, InOwners, Bytes, std::prev(RecentBlocks.end())});
			BlockBytes += Bytes;
		}
		return Result;
	}
};

FInstanceDataCache::FInstanceDataCache(FRenderBatchLimits InLimits) : Impl(std::make_unique<FImpl>(InLimits))
{
}

FInstanceDataCache::~FInstanceDataCache() = default;

std::shared_ptr<const FInstanceBatchData> FInstanceDataCache::Pack(const FRenderSceneSnapshot& InSnapshot,
                                                                   std::span<const std::size_t> InItems,
                                                                   FRenderBatchStats& OutStats)
{
	if (InItems.empty() || InItems.size() > UINT32_MAX)
	{
		throw std::invalid_argument("Invalid instance batch size");
	}
	auto Result = std::make_shared<FInstanceBatchData>();
	Result->InstanceCount = static_cast<std::uint32_t>(InItems.size());
	const auto& First = InSnapshot.Items.At(InItems.front());
	const auto Program = First.Preparation && First.Preparation->Program ? First.Preparation->Program
	                                                                     : First.State.Surface->GetCompiled();
	if (!Program)
	{
		throw std::invalid_argument("Instance packing requires a ready material program");
	}
	const auto& Pass = Program->GetPass(InSnapshot.View.Usage, "Instance");
	bool bStable = true;
	for (const auto Index : InItems)
	{
		const auto& Item = InSnapshot.Items.At(Index);
		Result->Owners.push_back(Item.Lifetime ? Item.Lifetime : Item.State.Resource);
		bStable &= Item.LocalItemId.has_value() && bool(Item.Lifetime);
	}
	for (std::uint32_t Slot = 0; Slot < Pass.Bindings.size(); ++Slot)
	{
		const auto& Binding = Pass.Bindings[Slot];
		if (!Binding.InstanceStride)
		{
			continue;
		}
		if (InItems.size() > Binding.InstanceCapacity)
		{
			throw std::invalid_argument("Instance batch exceeds shader capacity");
		}
		const auto Layout = Impl->Layout(Program, Pass, Slot);
		std::vector<FPackedRecord> Records;
		Records.reserve(InItems.size());
		for (const auto Index : InItems)
		{
			const auto& Item = InSnapshot.Items.At(Index);
			const auto ItemProgram = Item.Preparation && Item.Preparation->Program ? Item.Preparation->Program
			                                                                       : Item.State.Surface->GetCompiled();
			if (!ItemProgram)
			{
				throw std::invalid_argument("Instance packing requires a ready material program");
			}
			if (ItemProgram == Program)
			{
				Records.push_back(Impl->Record(Item, Binding, *Layout, OutStats));
				continue;
			}
			const auto& ItemPass = ItemProgram->GetPass(InSnapshot.View.Usage, "Instance");
			const auto ItemLayout = Impl->Layout(ItemProgram, ItemPass, Slot);
			if (ItemLayout != Layout && !ItemLayout->Matches(*Layout))
			{
				throw std::invalid_argument("Incompatible instance record layout");
			}
			Records.push_back(Impl->Record(Item, ItemPass.Bindings.at(Slot), *Layout, OutStats));
		}
		Result->Constants.push_back({Slot, Impl->Block(*Layout, Records, Result->Owners, bStable, OutStats)});
	}
	return Result;
}

void FInstanceDataCache::Collect()
{
	std::erase_if(Impl->Layouts,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Program.expired();
	              });
	for (auto It = Impl->Records.begin(); It != Impl->Records.end();)
	{
		if (It->second.Owner.expired())
		{
			Impl->EraseRecord(It++);
		}
		else
		{
			++It;
		}
	}
	for (auto It = Impl->Blocks.begin(); It != Impl->Blocks.end();)
	{
		if (std::any_of(It->second.Owners.begin(), It->second.Owners.end(),
		                [](const auto& InOwner)
		                {
			                return InOwner.expired();
		                }))
		{
			Impl->EraseBlock(It++);
		}
		else
		{
			++It;
		}
	}
}

void FInstanceDataCache::Clear()
{
	Impl = std::make_unique<FImpl>(Impl->Limits);
}

std::size_t FInstanceDataCache::ByteSize() const
{
	return Impl->RecordBytes + Impl->BlockBytes;
}
} // namespace Hyperion
