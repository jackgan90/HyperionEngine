#include "Hyperion/AssetEditing/AssetProperties.h"
#include "AssetOperations.h"

namespace Hyperion
{
FArchiveNode FAssetAutomation::ReadField(const std::string& InDocument, std::string_view InType,
                                         std::string_view InField)
{
	const auto Entry = Find(InDocument);
	if (Entry->Document->Loaded().Header.TypeId != InType)
	{
		throw FAutomationError("unsupported_type", "Operation requires a " + std::string(InType) + " document");
	}
	return Entry->Document->Get(InField);
}

TPendingOperation<FAssetDocumentInfo> FAssetAutomation::SetField(const std::string& InDocument,
                                                                 std::uint64_t InGeneration, std::string_view InType,
                                                                 std::string InField, FArchiveNode InValue)
{
	auto Entry = Edit(InDocument, InGeneration);
	(void)ReadField(InDocument, InType, InField);
	auto Prepared = PrepareAssetField(*Entry->Document, InField, std::move(InValue));
	std::vector<TAsyncResult<FAssetGraph>> Graphs;
	std::erase_if(Work,
	              [](const auto& InTask)
	              {
		              return InTask.Ready();
	              });
	Graphs.reserve(Prepared.References.size());
	Work.reserve(Work.size() + Prepared.References.size());
	for (const auto& Reference : Prepared.References)
	{
		auto Graph = Assets.LoadGraphAsync(Reference.Reference, Entry->Path);
		Work.push_back(Graph.Task());
		Graphs.push_back(std::move(Graph));
	}
	Entry->bEditing = true;
	if (Workspace)
	{
		Workspace->SetExternalEditing(Entry->Id, true);
	}
	return {
	    [this, Entry, InGeneration, Field = std::move(InField), Prepared = std::move(Prepared),
	     Graphs = std::move(Graphs)]() -> std::optional<FAssetDocumentInfo>
	    {
		    if (std::any_of(Graphs.begin(), Graphs.end(),
		                    [](const auto& InGraph)
		                    {
			                    return !InGraph.Ready();
		                    }))
		    {
			    return {};
		    }
		    Entry->bEditing = false;
		    if (Workspace)
		    {
			    Workspace->SetExternalEditing(Entry->Id, false);
			    const auto Current = Workspace->FindDocument(Entry->Id);
			    if (!Current || Current->Document != Entry->Document)
			    {
				    throw FAutomationError("stale_document", "Workspace document closed while preparing references");
			    }
		    }
		    if (Entry->Document->Generation() != InGeneration)
		    {
			    throw FAutomationError("stale_revision", "Asset changed while preparing references");
		    }
		    for (std::size_t Index = 0; Index < Graphs.size(); ++Index)
		    {
			    const auto& Required = Prepared.References[Index];
			    ValidateAssetReferenceGraph(*Graphs[Index].GetReady(), Required.Reference.TypeId, Required.Dimension);
		    }
		    CommitAssetField(*Entry->Document, Field, Prepared.Value);
		    return Describe(*Entry);
	    }};
}

std::vector<FModelPrimitiveInfo> FAssetAutomation::ModelPrimitives(const FAssetMutationRequest& InRequest)
{
	const auto Entry = Find(InRequest.Document);
	if (Entry->Document->Generation() != InRequest.Generation)
	{
		throw FAutomationError("stale_revision", "Asset changed; restart the property query");
	}
	return DescribeModelPrimitives(*Entry->Document);
}

FAssetDocumentInfo FAssetAutomation::SetPrimitives(const FAssetMutationRequest& InRequest, std::uint32_t InOffset,
                                                   const std::vector<FModelPrimitiveInfo>& InValues)
{
	const auto Entry = Edit(InRequest.Document, InRequest.Generation);
	auto Values = DescribeModelPrimitives(*Entry->Document);
	if (InValues.empty() || InValues.size() > 100 || InOffset > Values.size() ||
	    InValues.size() > Values.size() - InOffset)
	{
		throw std::invalid_argument("Replace 1-100 existing primitive entries");
	}
	std::copy(InValues.begin(), InValues.end(), Values.begin() + InOffset);
	SetModelPrimitives(*Entry->Document, Values);
	return Describe(*Entry);
}
} // namespace Hyperion
