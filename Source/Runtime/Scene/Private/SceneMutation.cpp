#include "SceneInternal.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <type_traits>

namespace Hyperion
{
FSceneStorage::FMutation::FMutation(FSceneStorage& InStorage)
    : Storage(InStorage), Settings(InStorage.Settings), NextSerial(InStorage.NextSerial),
      SlotCount(static_cast<std::uint32_t>(InStorage.Slots.size()))
{
}

FSceneStorage::FSlot& FSceneStorage::FMutation::Edit(std::uint32_t InSlot)
{
	const auto Found = Slots.find(InSlot);
	if (Found != Slots.end())
	{
		return *Found->second;
	}
	return *Slots
	            .emplace(InSlot, InSlot < Storage.Slots.size() ? std::make_unique<FSlot>(*Storage.Slots[InSlot])
	                                                           : std::make_unique<FSlot>())
	            .first->second;
}

const FSceneStorage::FSlot& FSceneStorage::FMutation::Read(std::uint32_t InSlot) const
{
	const auto Found = Slots.find(InSlot);
	return Found == Slots.end() ? *Storage.Slots.at(InSlot) : *Found->second;
}

void FSceneStorage::FMutation::Mark(std::uint32_t InSlot, ESceneChangeMask InMask)
{
	Masks[InSlot] |= InMask;
}

void FSceneStorage::FMutation::Unlink(std::uint32_t InSlot)
{
	const auto Parent = Read(InSlot).Parent;
	if (Parent == InvalidSlot)
	{
		if (!Roots)
		{
			Roots = Storage.Roots;
		}
		std::erase(*Roots, InSlot);
	}
	else
	{
		std::erase(Edit(Parent).Children, InSlot);
	}
}

void FSceneStorage::FMutation::Link(std::uint32_t InSlot, std::uint32_t InParent)
{
	auto& Entry = Edit(InSlot);
	Entry.Parent = InParent;
	Entry.Node->Parent() = InParent == InvalidSlot ? std::string{} : Read(InParent).Node->Id;
	if (InParent == InvalidSlot)
	{
		if (!Roots)
		{
			Roots = Storage.Roots;
		}
		Roots->push_back(InSlot);
	}
	else
	{
		Edit(InParent).Children.push_back(InSlot);
	}
}

std::uint32_t FSceneStorage::FMutation::Add(FSceneNode InNode, std::uint32_t InParent)
{
	if (InNode.Id.empty())
	{
		do
		{
			if (NextSerial == std::numeric_limits<std::uint64_t>::max())
			{
				throw std::overflow_error("Scene node ID serial exhausted");
			}
			InNode.Id = "node-" + std::to_string(NextSerial++);
		} while (Storage.Ids.contains(InNode.Id) || AddedIds.contains(InNode.Id));
	}
	if (Storage.Ids.contains(InNode.Id) || AddedIds.contains(InNode.Id))
	{
		throw std::invalid_argument("Duplicate scene node ID: " + InNode.Id);
	}
	if (InNode.Id.starts_with("node-"))
	{
		std::uint64_t Serial{};
		const auto Text = std::string_view(InNode.Id).substr(5);
		const auto Parsed = std::from_chars(Text.data(), Text.data() + Text.size(), Serial);
		if (Parsed.ec == std::errc{} && Parsed.ptr == Text.data() + Text.size() &&
		    Serial < std::numeric_limits<std::uint64_t>::max())
		{
			NextSerial = std::max(NextSerial, Serial + 1);
		}
	}
	if (InNode.Name.empty())
	{
		InNode.Name = InNode.Id;
	}
	ValidateSceneNode(InNode);
	if (!Free)
	{
		Free = Storage.Free;
	}
	std::uint32_t Index{};
	if (Free->empty())
	{
		if (SlotCount == InvalidSlot)
		{
			throw std::length_error("Scene slot capacity exhausted");
		}
		Index = SlotCount++;
	}
	else
	{
		Index = Free->back();
		Free->pop_back();
	}
	auto& Entry = Edit(Index);
	if (Entry.Generation == std::numeric_limits<std::uint64_t>::max())
	{
		throw std::overflow_error("Scene handle generation exhausted");
	}
	++Entry.Generation;
	Entry.Node = std::move(InNode);
	Entry.Children.clear();
	AddedIds.emplace(Entry.Node->Id, Index);
	if (!Order)
	{
		Order = Storage.Order;
	}
	Order->push_back(Index);
	Link(Index, InParent);
	Mark(Index, ESceneChangeMask::Structure);
	return Index;
}

void FSceneStorage::FMutation::Derive(std::uint32_t InRoot)
{
	std::vector<std::uint32_t> Pending{InRoot};
	for (std::size_t Index = 0; Index < Pending.size(); ++Index)
	{
		const auto Slot = Pending[Index];
		auto& Entry = Edit(Slot);
		const auto World =
		    Entry.Parent == InvalidSlot ? Entry.Node->Local() : Multiply(Read(Entry.Parent).World, Entry.Node->Local());
		const bool bEnabled =
		    Entry.Node->bEnabled && (Entry.Parent == InvalidSlot || Read(Entry.Parent).bEffectiveEnabled);
		ValidateSceneWorld(*Entry.Node, World);
		if (Entry.World.Values != World.Values)
		{
			Mark(Slot, ESceneChangeMask::Transform);
		}
		if (Entry.bEffectiveEnabled != bEnabled)
		{
			Mark(Slot, ESceneChangeMask::Enabled);
		}
		Entry.World = World;
		Entry.bEffectiveEnabled = bEnabled;
		Pending.insert(Pending.end(), Entry.Children.begin(), Entry.Children.end());
	}
}

void FSceneStorage::FMutation::Remove(std::uint32_t InSlot)
{
	auto& Entry = Edit(InSlot);
	const FSceneHandle Handle{Storage.Identity, InSlot, Entry.Generation};
	for (auto* Selection : {&Settings.DefaultCamera, &Settings.MainDirectionalLight, &Settings.EnvironmentLight})
	{
		if (*Selection == Handle)
		{
			Selection->reset();
		}
	}
	RemovedIds.push_back(Entry.Node->Id);
	Entry.Node.reset();
	Entry.Children.clear();
	Entry.Parent = InvalidSlot;
	Entry.World = Hyperion::Identity();
	Entry.bEffectiveEnabled = false;
	Entry.Transfer = {};
	if (!Order)
	{
		Order = Storage.Order;
	}
	if (!Free)
	{
		Free = Storage.Free;
	}
	Free->push_back(InSlot);
	Mark(InSlot, ESceneChangeMask::Structure);
}

std::map<FSceneHandle, FSceneChange> FSceneStorage::FMutation::PrepareChanges(std::uint64_t InRevision,
                                                                              bool bInSettingsChanged)
{
	std::map<FSceneHandle, FSceneChange> PendingChanges;
	for (const auto& [Index, Mask] : Masks)
	{
		auto& Entry = Edit(Index);
		if (Entry.Node && Entry.Node->Model())
		{
			Entry.Transfer = SceneModelTransfer(*Entry.Node, Entry.World, Entry.bEffectiveEnabled);
		}
		FSceneChange Change;
		Change.Handle = {Storage.Identity, Index, Entry.Generation};
		Change.Revision = InRevision;
		Change.Mask = Mask;
		if (const auto Old = Storage.Changes.find(Change.Handle); Old != Storage.Changes.end())
		{
			Change.Mask |= Old->second.Mask;
		}
		Change.bRemoved = !Entry.Node;
		Change.Kind = Entry.Node ? Entry.Node->GetKind() : Storage.Slots.at(Index)->Node->GetKind();
		Change.Node = Entry.Node;
		Change.World = Entry.World;
		Change.bEffectiveEnabled = Entry.bEffectiveEnabled;
		if (Entry.Node && Entry.Node->Model())
		{
			Change.Model = Entry.Transfer;
		}
		PendingChanges.emplace(Change.Handle, std::move(Change));
	}
	if (bInSettingsChanged)
	{
		FSceneChange Change;
		Change.Handle = {Storage.Identity, InvalidSlot, 0};
		Change.Revision = InRevision;
		Change.Mask = ESceneChangeMask::Settings;
		Change.Settings = Settings;
		PendingChanges.emplace(Change.Handle, std::move(Change));
	}
	return PendingChanges;
}

void FSceneStorage::FMutation::Commit(bool bInAdvanceRevision, bool bInForceSettings)
{
	const bool bSettingsChanged = Settings != Storage.Settings;
	if (Masks.empty() && !bSettingsChanged && !bInForceSettings)
	{
		return;
	}
	if (bInAdvanceRevision && Storage.Revision == std::numeric_limits<std::uint64_t>::max())
	{
		throw std::overflow_error("Scene revision exhausted");
	}
	const auto PublishedRevision = Storage.Revision + (bInAdvanceRevision ? 1 : 0);
	auto PendingChanges = PrepareChanges(PublishedRevision, bSettingsChanged || bInForceSettings);
	if (Order && !RemovedIds.empty())
	{
		std::erase_if(*Order,
		              [&](std::uint32_t InSlot)
		              {
			              const auto Entry = Slots.find(InSlot);
			              return Entry != Slots.end() && !Entry->second->Node;
		              });
	}
	// All value validation and allocations finish before the first authoritative write.
	Storage.Slots.reserve(SlotCount);
	static_assert(std::is_nothrow_swappable_v<std::unique_ptr<FSlot>>);
	while (Storage.Slots.size() < SlotCount)
	{
		Storage.Slots.emplace_back();
	}
	for (auto& [Index, Entry] : Slots)
	{
		auto& Previous = Storage.Slots[Index];
		for (std::size_t Kind = 0; Kind < Storage.Counts.size(); ++Kind)
		{
			const auto Capability = static_cast<ESceneNodeKind>(Kind);
			if (Previous && Previous->Node && Previous->Node->Has(Capability))
			{
				--Storage.Counts[Kind];
			}
			if (Entry->Node && Entry->Node->Has(Capability))
			{
				++Storage.Counts[Kind];
			}
		}
		std::swap(Previous, Entry);
	}
	for (const auto& Id : RemovedIds)
	{
		Storage.Ids.erase(Id);
	}
	Storage.Ids.merge(AddedIds);
	for (const auto& [Handle, Change] : PendingChanges)
	{
		Storage.Changes.erase(Handle);
	}
	Storage.Changes.merge(PendingChanges);
	if (Roots)
	{
		Storage.Roots = std::move(*Roots);
	}
	if (Order)
	{
		Storage.Order = std::move(*Order);
	}
	if (Free)
	{
		Storage.Free = std::move(*Free);
	}
	Storage.Settings = Settings;
	Storage.NextSerial = NextSerial;
	Storage.Revision = PublishedRevision;
}
} // namespace Hyperion
