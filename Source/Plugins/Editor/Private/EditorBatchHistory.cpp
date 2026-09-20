#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::CommitEdits(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision,
                                std::uint64_t InInteraction)
{
	if (!InInteraction)
	{
		FinishInspectorEdit();
	}
	if (Scene->GetRevision() != InExpectedRevision)
	{
		throw std::runtime_error("The objects changed while editing. Try again with their current values.");
	}
	FHistoryEntry Entry;
	Entry.BeforeSettings = Scene->GetSettings();
	Entry.BeforeState = DocumentState;
	std::vector<FSceneHandle> Targets;
	bool bChanged{};
	for (const auto& [Handle, Candidate] : InEdits)
	{
		const auto* Before = Scene->FindNode(Handle);
		if (!Before)
		{
			throw std::runtime_error("An edit target is no longer current");
		}
		bChanged |= *Before != Candidate;
		Targets.push_back(Handle);
		Entry.Edits.push_back({Handle, *Before, Candidate});
	}
	if (InEdits.empty() || (!bChanged && !InInteraction))
	{
		return;
	}
	const bool bMerge = InInteraction && InspectorTransaction && InspectorTransaction->Interaction == InInteraction &&
	                    InspectorTransaction->Targets == Targets &&
	                    InspectorTransaction->Revision == InExpectedRevision && HistoryCursor == History.size() &&
	                    InspectorTransaction->HistoryIndex + 1 == HistoryCursor;
	History.reserve(HistoryCursor + 1);
	if (!Scene->EditNodes(std::move(InEdits), InExpectedRevision))
	{
		throw std::runtime_error("The edit targets are no longer current");
	}
	Entry.AfterSettings = Scene->GetSettings();
	Entry.AfterState = ++NextDocumentState;
	DocumentState = Entry.AfterState;
	if (bMerge)
	{
		auto& Previous = History.back();
		for (std::size_t Index = 0; Index < Entry.Edits.size(); ++Index)
		{
			Previous.Edits[Index].After = std::move(Entry.Edits[Index].After);
		}
		Previous.AfterSettings = std::move(Entry.AfterSettings);
		Previous.AfterState = Entry.AfterState;
	}
	else
	{
		History.resize(HistoryCursor);
		History.push_back(std::move(Entry));
		++HistoryCursor;
	}
	if (InInteraction)
	{
		InspectorTransaction = FInspectorTransaction{InInteraction, Targets.front(), HistoryCursor - 1,
		                                             Scene->GetRevision(), std::move(Targets)};
	}
	Error.clear();
}
} // namespace Hyperion
