#pragma once
#include "EditorSelection.h"

namespace Hyperion
{
// Main-owned drafts contain projected values, never borrowed pointers into a mutable scene.
class FEditorInspectionCache
{
public:
	struct FSelectionEntry
	{
		std::shared_ptr<const FSceneComponentDescriptor> Type;
		std::unique_ptr<FRecordSelectionDraft> Draft;
		std::vector<std::string> Components;
		std::vector<std::string> Blocked;
	};

	void Prepare(std::uint64_t InDocument, std::uint64_t InRevision, std::span<const FSceneHandle> InTargets)
	{
		if (Document != InDocument || Revision != InRevision || !std::ranges::equal(Targets, InTargets))
		{
			Clear();
			Targets.assign(InTargets.begin(), InTargets.end());
			Document = InDocument;
			Revision = InRevision;
		}
	}

	FRecordDraft& Single(const FSceneComponent& InComponent)
	{
		auto& Entry = Singles[InComponent.Id];
		if (Entry.Type != InComponent.Type || !Entry.Draft)
		{
			auto Draft = std::make_unique<FRecordDraft>(*InComponent.Type->Record, InComponent.Get());
			Entry.Type = InComponent.Type;
			Entry.Draft = std::move(Draft);
		}
		return *Entry.Draft;
	}

	std::unique_ptr<FRecordDraft> TakeSingle(const std::string& InComponent)
	{
		return std::move(Singles.at(InComponent).Draft);
	}

	FSelectionEntry* FindSelection(const FSceneComponentDescriptor& InType)
	{
		const auto Found = Selections.find(InType.Id);
		return Found != Selections.end() && Found->second.Type.get() == &InType && Found->second.Draft ? &Found->second
		                                                                                               : nullptr;
	}

	FSelectionEntry& StoreSelection(FSelectionEntry InEntry)
	{
		const auto Id = InEntry.Type->Id;
		return Selections.insert_or_assign(Id, std::move(InEntry)).first->second;
	}

	std::unique_ptr<FRecordSelectionDraft> TakeSelection(const std::string& InType)
	{
		return std::move(Selections.at(InType).Draft);
	}

	void Clear()
	{
		Singles.clear();
		Selections.clear();
		Targets.clear();
	}

private:
	struct FSingleEntry
	{
		std::shared_ptr<const FSceneComponentDescriptor> Type;
		std::unique_ptr<FRecordDraft> Draft;
	};

	std::uint64_t Document{};
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Targets;
	std::map<std::string, FSingleEntry> Singles;
	std::map<std::string, FSelectionEntry> Selections;
};
} // namespace Hyperion
