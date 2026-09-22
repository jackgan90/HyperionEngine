#include "AssetPropertyWidgets.h"
#include "AssetWorkspace.h"

namespace Hyperion
{
bool FAssetWorkspace::EditReference(FGui& InGui, const char* InLabel, FAssetRef& InReference, std::string_view InType,
                                    std::optional<ETextureDimension> InDimension)
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
		Active->ReferenceEdit = FReferenceSelection{Active->Document->Generation(), std::string(InType), InDimension,
		                                            Assets.LoadGraphAsync(Reference, Active->Path)};
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

void FAssetWorkspace::CommitReferenceEdit(FEntry& InEntry, std::string InField, FArchiveNode InCandidate)
{
	if (InEntry.ReferenceEdit)
	{
		InEntry.ReferenceEdit->Field = std::move(InField);
		InEntry.ReferenceEdit->Candidate = std::move(InCandidate);
	}
}

void FAssetWorkspace::PollReferenceEdit(FEntry& InEntry)
{
	if (!InEntry.ReferenceEdit || !InEntry.ReferenceEdit->Request.Ready())
	{
		return;
	}
	auto Edit = std::move(*InEntry.ReferenceEdit);
	InEntry.ReferenceEdit.reset();
	try
	{
		if (Edit.Generation != InEntry.Document->Generation() || Edit.Field.empty())
		{
			throw std::runtime_error("Reference edit was superseded; select the asset again");
		}
		const auto Graph = Edit.Request.GetReady();
		if (!Graph->Root || !Graph->Failures.empty())
		{
			throw std::runtime_error("Selected asset has invalid dependencies");
		}
		if (Graph->Root->Header.TypeId != Edit.Type)
		{
			throw std::runtime_error("Selected asset has a different type");
		}
		if (Edit.Dimension && Graph->Root->As<FTextureAsset>()->Dimension != *Edit.Dimension)
		{
			throw std::runtime_error("Texture dimension does not match the material parameter");
		}
		InEntry.Document->Set(std::move(Edit.Field), std::move(Edit.Candidate));
	}
	catch (const std::exception& Failure)
	{
		InEntry.bSaveRequested = false;
		InEntry.Document->Error = Failure.what();
	}
}
} // namespace Hyperion
