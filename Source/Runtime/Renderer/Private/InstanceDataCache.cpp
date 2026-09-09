#include "InstanceDataCache.h"
#include "Hyperion/Renderer/MaterialInputValues.h"
#include "Hyperion/Renderer/MaterialPacking.h"
#include "SceneItemPreparation.h"
#include <algorithm>
#include <list>
#include <map>
#include <tuple>

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
	std::map<FRecordKey, FRecord> Records;
	std::list<FRecordKey> RecentRecords;
	std::map<FBlockKey, FBlock> Blocks;
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

	void EraseRecord(std::map<FRecordKey, FRecord>::iterator InEntry)
	{
		RecordBytes -= InEntry->second.Bytes;
		RecentRecords.erase(InEntry->second.Recent);
		Records.erase(InEntry);
	}

	void EraseBlock(std::map<FBlockKey, FBlock>::iterator InEntry)
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
		std::vector<std::shared_ptr<const FMaterialValue>> Values;
		for (const auto& Member : InBinding.Members)
		{
			Values.push_back(InItem.GetMaterialValue(Member.ParameterIndex));
		}
		const bool bStable = InItem.LocalItemId.has_value() && bool(InItem.Lifetime);
		const auto Existing = Records.find(Key);
		if (bStable && Existing != Records.end() && SameValues(Existing->second.Values, Values))
		{
			Existing->second.Owner = InItem.Lifetime;
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
			Records.emplace(Key,
			                FRecord{Result, std::move(Values), InItem.Lifetime, Bytes, std::prev(RecentRecords.end())});
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
