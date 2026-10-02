#include "AssetPropertyWidgets.h"
#include "AssetWorkspace.h"
#include "Hyperion/AssetEditing/AssetProperties.h"

namespace Hyperion
{
bool FAssetWorkspace::EditReference(FGui& InGui, const char* InLabel, FAssetRef& InReference, std::string_view InType)
{
	try
	{
		auto [It, bInserted] = ReferenceChoices.try_emplace(std::string(InType));
		auto& Choices = It->second;
		if (bInserted)
		{
			for (const auto& Reference : AssetIndex)
			{
				if (Reference.TypeId == InType)
				{
					Choices.Indices.emplace(Reference.Id, Choices.References.size());
					Choices.Labels.push_back(Reference.Path);
					Choices.References.push_back(Reference);
				}
			}
		}
		const auto Current = Choices.Indices.find(InReference.Id);
		std::size_t Selected = Current == Choices.Indices.end() ? Choices.References.size() : Current->second;
		const bool bChanged = AssetCombo(
		    InGui, InLabel, Choices.Labels, Selected,
		    [&](std::size_t InIndex, FVec4)
		    {
			    ObserveProperty(InGui, "choice/" + std::string(InLabel) + Choices.Labels[InIndex]);
		    },
		    InReference.Path.empty() ? "None" : InReference.Path.c_str());
		ObserveProperty(InGui, InLabel);
		if (!bChanged || Active->HasPendingEdit())
		{
			return false;
		}
		const auto& Reference = Choices.References.at(Selected);
		Active->ReferenceEdit = FReferenceSelection{Active->Document->Generation()};
		InReference = Reference;
		return true;
	}
	catch (const std::exception& Failure)
	{
		Active->Document->Error = Failure.what();
		Active->ReferenceEdit.reset();
		return false;
	}
}

void FAssetWorkspace::CommitReferenceEdit(FEntry& InEntry, const FRecordMemberIdentity& InField,
                                          FArchiveNode InCandidate)
{
	if (!InEntry.ReferenceEdit)
	{
		return;
	}
	const auto Generation = InEntry.ReferenceEdit->Generation;
	InEntry.ReferenceEdit.reset();
	try
	{
		InEntry.EditWorkflow =
		    FAssetEditWorkflow::Field(Tasks, Assets, InEntry.Document, Generation, InField, std::move(InCandidate));
	}
	catch (const std::exception& Failure)
	{
		InEntry.Document->Error = Failure.what();
	}
}

void FAssetWorkspace::PollEditWorkflow(FEntry& InEntry)
{
	if (!InEntry.EditWorkflow)
	{
		return;
	}
	try
	{
		if (InEntry.EditWorkflow->Poll(InEntry.Document))
		{
			InEntry.EditWorkflow.reset();
		}
	}
	catch (const std::exception& Failure)
	{
		InEntry.EditWorkflow.reset();
		InEntry.bSaveRequested = false;
		InEntry.Document->Error = Failure.what();
	}
}
} // namespace Hyperion
