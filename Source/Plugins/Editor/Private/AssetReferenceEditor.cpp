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
		InGui.BeginDisabled(Active->HasPendingEdit());
		const bool bChanged = AssetCombo(
		    InGui, InLabel, Choices.Labels, Selected,
		    [&](std::size_t InIndex, FVec4)
		    {
			    ObserveProperty(InGui, "choice/" + std::string(InLabel) + Choices.Labels[InIndex]);
		    },
		    InReference.Path.empty() ? "None" : InReference.Path.c_str());
		InGui.EndDisabled();
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

void FAssetWorkspace::SubmitFieldEdit(FEntry& InEntry, const FRecordMemberIdentity& InField, FArchiveNode InCandidate,
                                      std::uint64_t InInteraction)
{
	const auto Generation = InEntry.ReferenceEdit ? InEntry.ReferenceEdit->Generation : InEntry.Document->Generation();
	InEntry.ReferenceEdit.reset();
	try
	{
		if (InEntry.EditWorkflow)
		{
			InEntry.EditWorkflow->UpdateField(InField, std::move(InCandidate), InInteraction);
		}
		else
		{
			InEntry.EditWorkflow = FAssetEditWorkflow::SubmitField(Tasks, Assets, InEntry.Document, Generation, InField,
			                                                       std::move(InCandidate), InInteraction);
		}
	}
	catch (const std::exception& Failure)
	{
		InEntry.Document->Error = Failure.what();
	}
}

void FAssetWorkspace::UpdateFieldInteraction(FGui& InGui, FEntry& InEntry, std::uint64_t InPreviousInteraction)
{
	const auto PendingInteraction = InEntry.EditWorkflow ? InEntry.EditWorkflow->Interaction() : 0;
	const auto Interaction = PendingInteraction ? PendingInteraction : InPreviousInteraction;
	const auto Pointer = InGui.PointerState();
	if (Interaction && Pointer.bCancel)
	{
		if (PendingInteraction)
		{
			InEntry.EditWorkflow->CancelInteraction(Interaction);
		}
		else
		{
			InEntry.Document->CancelInteraction(Interaction);
		}
		InEntry.GuiInteraction = 0;
		InEntry.EditingParameter.clear();
		InGui.FinishEditing();
	}
	else if (PendingInteraction)
	{
		if (InEntry.GuiInteraction != PendingInteraction)
		{
			InEntry.EditWorkflow->FinishInteraction();
			InEntry.EditingParameter.clear();
		}
	}
	else if (InPreviousInteraction && !InEntry.GuiInteraction)
	{
		InEntry.Document->FinishInteraction();
		InEntry.EditingParameter.clear();
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
